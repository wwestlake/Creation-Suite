"""Order 1 acceptance against actual Debug apps and the existing VFS service.

Uses unique disposable projects, real Station import/placement/save paths and
Movie's authenticated command API. No native pointer-input claim. Owned app
processes are closed in finally; no existing application/service is terminated.
"""
import argparse
import array
import copy
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser()
parser.add_argument('--heartbeat', type=Path, required=True)
parser.add_argument('--ffmpeg', default=shutil.which('ffmpeg'))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
movie_root = root / 'apps/CreationMovie'
station_root = root / 'apps/CreationStation'
run = movie_root / 'build' / ('order1-check-' + uuid.uuid4().hex[:8])
run.mkdir(parents=True)
env = dict(os.environ, TEMP=str(run), TMP=str(run), PYTHONDONTWRITEBYTECODE='1')
vfs = 'http://127.0.0.1:' + str(json.loads(args.heartbeat.read_text())['httpPort'])
owned, created, checks = [], {}, []
base = token = None
movie_process = None
settings_backup = {}
completed = False
metrics = {}


def http(url, body=None, method=None, authenticated=False, raw=False, extra_headers=None):
    headers = dict(extra_headers or {})
    if authenticated:
        headers['Authorization'] = 'Bearer ' + token
    if body is not None and not isinstance(body, bytes):
        body = json.dumps(body).encode()
        headers['Content-Type'] = 'application/json'
    elif isinstance(body, bytes):
        headers.setdefault('Content-Type', 'application/octet-stream')
    request = urllib.request.Request(url, body, headers, method=method)
    with urllib.request.urlopen(request, timeout=20) as response:
        data = response.read()
    return data if raw else (json.loads(data) if data else None)


def entry(project, path):
    return http(vfs + '/project/entry?' + urllib.parse.urlencode({'projectId': project, 'path': path}), raw=True)


def manifest(project):
    return http(vfs + '/project/manifest?' + urllib.parse.urlencode({'projectId': project}))


def get(path):
    return http(base + path, authenticated=True)


def command(name, body=None):
    queued = http(base + '/v1/commands/' + name, body or {}, authenticated=True)
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        result = get('/v1/requests/' + queued['requestId'])
        if result['status'] in ('completed', 'failed'):
            assert result['details']['ok'], result
            return result['details'].get('data')
        time.sleep(.05)
    raise AssertionError('Command timeout: ' + name)


def job(result):
    deadline = time.monotonic() + 120
    while time.monotonic() < deadline:
        state = get('/v1/app/jobs')
        assert state['id'] == result['jobId'], 'Job replaced before completion'
        if state['status'] in ('completed', 'failed', 'cancelled'):
            assert state['status'] == 'completed', state
            return state['result']
        time.sleep(.1)
    raise AssertionError('Job timed out')


def passed(message):
    checks.append(message)
    print('PASS:', message, flush=True)


def launch(path, *arguments):
    process = subprocess.Popen([str(path), *map(str, arguments)], env=env)
    owned.append(process)
    return process


def station(request):
    output = run / ('station-result-' + uuid.uuid4().hex[:8] + '.json')
    request['output'] = str(output)
    input_path = output.with_name(output.stem + '-request.json')
    input_path.write_text(json.dumps(request))
    process = launch(station_root / 'build/CreativeWorkstation_artefacts/Debug/DjehutiStation.exe',
                     '--shared-project-check', input_path)
    process.wait(timeout=180)
    assert process.returncode == 0, 'Station exited abnormally'
    result = json.loads(output.read_text(encoding='utf-8-sig'))
    if request.get('mode') == 'create' and result.get('projectId'):
        candidate = manifest(result['projectId'])
        if candidate['projectName'] == request['name']:
            created[result['projectId']] = request['name']
    assert result['ok'], result['error']
    return result


def start_movie():
    global base, token, movie_process
    movie_process = launch(movie_root / 'build/CreationMovieApp_artefacts/Debug/Djehuti Movie.exe')
    deadline = time.monotonic() + 45
    while time.monotonic() < deadline:
        try:
            discovery = http(vfs + '/suite/entry?path=agents/movie.json')
            token, base = discovery['token'], discovery['baseUrl']
            get('/v1/status')
            assert movie_process.poll() is None
            return get('/v1/app/state')['projectId']
        except (OSError, ValueError, KeyError):
            time.sleep(.1)
    raise AssertionError('Movie did not start')


def stop_movie(original):
    if original:
        job(command('movie.project.open', {'projectId': original}))
    try:
        command('movie.app.close')
    except (OSError, urllib.error.URLError):
        # A clean close may remove the API before its queued response can be
        # polled. Process termination is the authoritative completion signal.
        movie_process.wait(timeout=30)
    movie_process.wait(timeout=30)
    assert movie_process.returncode == 0


def timeline(xml):
    def canonical(node):
        return node.tag, sorted(node.attrib.items()), (node.text or '').strip(), [canonical(x) for x in node]
    return canonical(ET.fromstring(xml))


def canonical_sequence(sequence):
    result = copy.deepcopy(sequence)
    result['media'].sort(key=lambda media: (media['id'], media['versionId']))
    return result


try:
    # Refuse to interfere with existing sessions. The VFS service is supplied by
    # the caller and is intentionally not owned/terminated by this script.
    running = subprocess.check_output(['powershell', '-NoProfile', '-Command',
        "Get-Process | Where-Object { $_.ProcessName -in @('DjehutiStation','Djehuti Station','Djehuti Movie') } | Select-Object -ExpandProperty Id"], text=True)
    assert not running.strip(), 'Close existing Station/Movie instances before testing'
    for path in ('station-settings.json', 'movie/layout.json'):
        try:
            settings_backup[path] = http(vfs + '/suite/entry?' + urllib.parse.urlencode({'path': path}), raw=True)
        except urllib.error.HTTPError as error:
            assert error.code == 404
            settings_backup[path] = None
    assert args.ffmpeg, 'Provide an existing ffmpeg executable'
    audio, video = run / 'tone.wav', run / 'picture.mp4'
    for destination, inputs in [(audio, ['-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000:duration=3', '-ac', '2']),
                                (video, ['-f', 'lavfi', '-i', 'testsrc2=size=320x180:rate=30:duration=3', '-c:v', 'libx264', '-pix_fmt', 'yuv420p'])]:
        with (run / 'source-generation.log').open('ab') as log:
            subprocess.run([args.ffmpeg, '-y', '-hide_banner', '-loglevel', 'error', *inputs, str(destination)],
                           env=env, stdout=log, stderr=log, check=True, timeout=30)
    tag = 'Order 1 acceptance ' + str(uuid.uuid4())
    initial = station({'mode': 'create', 'name': tag, 'audio': str(audio), 'video': str(video)})
    project = initial['projectId']
    created[project] = tag
    station_state = entry(project, 'session.xml')
    (run / 'station-state.xml').write_bytes(station_state)
    original_catalog = {(a['id'], a['versionId'], a['logicalPath']) for a in manifest(project)['assetCatalog']}
    assert initial['clipCount'] >= 2
    try:
        entry(project, 'Project/movie-state.json')
        raise AssertionError('Station project unexpectedly has a Movie sequence')
    except urllib.error.HTTPError as error:
        assert error.code == 404
    passed('Station imports real audio/video, registers catalog assets and saves native timeline references')

    original_movie = start_movie()
    job(command('movie.project.open', {'projectId': project}))
    state = get('/v1/app/state')
    assert state['projectId'] == project and not state['sequence']['clips']
    available = {(m['id'], m['versionId'], m['path']) for m in state['sequence']['media']}
    assert original_catalog <= available
    passed('Movie opens Station project with no preexisting Movie state and resolves original catalog identities/paths')
    for media in state['sequence']['media']:
        command('movie.timeline.place', {'assetId': media['id'], 'versionId': media['versionId'], 'start': 0, 'duration': 1.0})
    edited = get('/v1/app/sequence')
    owner = edited['clips'][0]['trackId']
    command('movie.track.rename', {'trackId': owner, 'name': 'Movie editorial metadata'})
    # Add a genuinely different version through the existing catalog/VFS API
    # while Movie has unsaved edits. Refresh/undo must preserve both the edits
    # and the independently registered asset version.
    latest = manifest(project)
    try:
        http(vfs + '/project/manifest?' + urllib.parse.urlencode({'projectId': project}), latest, method='PUT')
        raise AssertionError('Unguarded manifest replacement was accepted')
    except urllib.error.HTTPError as error:
        assert error.code == 428
    assert manifest(project)['revision'] == latest['revision']
    metrics['unguardedManifestWriteRejected'] = True
    audio_asset = next(a for a in latest['assetCatalog'] if a['kind'] == 'audio')
    second_tone = run / 'tone-v2.wav'
    subprocess.run([args.ffmpeg, '-y', '-v', 'error', '-f', 'lavfi', '-i',
        'sine=frequency=880:sample_rate=48000:duration=3', '-ac', '2', str(second_tone)], env=env, check=True, timeout=30)
    version = copy.deepcopy(audio_asset)
    version.update(versionId=audio_asset['id'] + '@2', version='2', revision=audio_asset.get('revision', 0) + 1,
                   logicalPath='Assets/Derived/order1-tone-v2.wav', modifiedAtMs=int(time.time() * 1000),
                   fileSizeBytes=second_tone.stat().st_size, externalSourcePath='', sourceTool='Order 1 version fixture')
    with second_tone.open('rb') as source:
        for offset in range(0, second_tone.stat().st_size, 256 * 1024):
            http(vfs + '/project/entry/chunk?' + urllib.parse.urlencode({'projectId': project,
                'path': version['logicalPath'], 'offset': offset, 'total': second_tone.stat().st_size}),
                source.read(256 * 1024), method='PUT')
    revision = latest['revision']
    latest['assetCatalog'].append(version)
    http(vfs + '/project/manifest?' + urllib.parse.urlencode({'projectId': project}), latest, method='PUT',
         extra_headers={'If-Match': str(revision)})
    job(command('movie.media.refresh'))
    current = get('/v1/app/sequence')
    assert any(m['versionId'] == version['versionId'] for m in current['media'])
    assert all(c['assetVersionId'] != version['versionId'] for c in current['clips'])
    command('movie.timeline.undo')
    assert any(m['versionId'] == version['versionId'] for m in get('/v1/app/sequence')['media'])
    command('movie.timeline.redo')
    passed('Refreshing a new shared asset version preserves unsaved edits, exact clip bindings and media across undo/redo')
    job(command('movie.project.save'))
    movie_state = entry(project, 'Project/movie-state.json')
    (run / 'movie-state.json').write_bytes(movie_state)
    assert entry(project, 'session.xml') == station_state
    job(command('movie.export', {'path': str(run / 'shared-project-preview.mkv'), 'preset': 'ffv1', 'width': 320, 'height': 180}))
    assert (run / 'shared-project-preview.mkv').stat().st_size > 1000
    pixels = subprocess.check_output([args.ffmpeg, '-v', 'error', '-i', str(run / 'shared-project-preview.mkv'),
        '-frames:v', '1', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], env=env, timeout=30)
    samples = array.array('f', subprocess.check_output([args.ffmpeg, '-v', 'error', '-i', str(run / 'shared-project-preview.mkv'),
        '-t', '0.5', '-vn', '-f', 'f32le', '-'], env=env, timeout=30))
    assert pixels and max(pixels) - min(pixels) > 30, 'Shared video rendered black'
    assert samples and max(map(abs, samples)) > .01, 'Shared audio rendered silence'
    mono = samples[::2]
    def component(hz):
        return abs(sum(x * complex(math.cos(2 * math.pi * hz * n / 48000), math.sin(2 * math.pi * hz * n / 48000))
                       for n, x in enumerate(mono)))
    metrics['originalToNewAudioToneRatio'] = component(440) / max(component(880), 1e-9)
    metrics['exportPeak'] = max(map(abs, samples))
    metrics['exportPixelRange'] = max(pixels) - min(pixels)
    assert metrics['originalToNewAudioToneRatio'] > 20, 'Export silently retargeted the clip to the newer audio version'
    passed('Movie edits and exports shared media while preserving Station session bytes')
    second_name = tag + ' Movie origin'
    second = job(command('movie.project.new', {'name': second_name}))['projectId']
    created[second] = second_name
    for path in (audio, video):
        job(command('movie.media.import', {'path': str(path)}))
    job(command('movie.project.save'))
    second_movie_state = entry(second, 'Project/movie-state.json')
    assert len(manifest(second)['assetCatalog']) == 2
    stop_movie(original_movie)

    returned = station({'mode': 'open', 'projectId': second, 'expectEmpty': True})
    assert returned['clipCount'] == 0 and len(returned['catalog']) == 2
    assert entry(second, 'Project/movie-state.json') == second_movie_state
    assert manifest(second)['appDomain'] == 'movie'
    passed('Station opens Movie-origin project with a fresh native workspace, resolves media and preserves Movie editing bytes')
    for cycle in range(3):
        returned = station({'mode': 'open', 'projectId': project})
        assert timeline(returned['timeline']) == timeline(initial['timeline'])
        assert entry(project, 'Project/movie-state.json') == movie_state
        original_movie = start_movie()
        job(command('movie.project.open', {'projectId': project}))
        current = get('/v1/app/sequence')
        assert canonical_sequence(json.loads(movie_state)) == canonical_sequence(current)
        assert get('/v1/app/state')['projectId'] == project
        current_station_state = entry(project, 'session.xml')
        job(command('movie.project.save'))
        assert entry(project, 'session.xml') == current_station_state
        assert original_catalog <= {(a['id'], a['versionId'], a['logicalPath']) for a in manifest(project)['assetCatalog']}
        stop_movie(original_movie)
        movie_state = entry(project, 'Project/movie-state.json')
    passed('Three repeated cross-application cycles preserve identity, exact asset versions, both timelines and native editing metadata')
    completed = True
finally:
    for process in reversed(owned):
        if process.poll() is None:
            # Only this run's explicit PIDs; includes any child workers of that app.
            subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], capture_output=True)
            process.wait(timeout=15)
    for project, name in created.items():
        current = manifest(project)
        assert current['projectName'] == name
        http(vfs + '/project?' + urllib.parse.urlencode({'projectId': project}), method='DELETE')
    for path, data in settings_backup.items():
        url = vfs + '/suite/entry?' + urllib.parse.urlencode({'path': path})
        try:
            http(url, data, method='PUT' if data is not None else 'DELETE', raw=True)
        except urllib.error.HTTPError as error:
            if error.code != 404 or data is not None:
                raise
    (run / 'results.json').write_text(json.dumps({'ok': completed, 'checks': checks, 'metrics': metrics,
        'allOwnedProcessesExited': all(p.poll() is not None for p in owned)}, indent=2))
    print('Evidence:', run, flush=True)
