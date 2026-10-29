# Open-ended project entry writes (version 1)

Movie recording uses the existing suite service as the only owner of media
storage. This API also supports other unknown-length project entries. It does
not introduce a second container or filesystem storage path.

`GET /project/stream/capabilities` advertises `version: 1` and a maximum write
piece of 4 MiB. Existing complete-entry and known-size chunk APIs are unchanged.

`PUT /project/stream` accepts query parameters `projectId`, `operationId`,
`action`, `path`, `offset`, and `revision`, and a raw binary request body.
Use a stable UUID as the operation ID. The public client is
`SuiteVfsServiceClient::streamProjectEntry`.

| Action | Contract |
| --- | --- |
| `begin` | Create a new safe destination at revision 0. A repeated begin for the same operation/path returns existing progress without truncation. Existing final destinations are never overwritten. |
| `write` | Write at an explicit offset no greater than the acknowledged high-water size. Header patches are permitted. Supply the current revision; success advances it once. |
| `inspect` | Return persisted state, size and revision without changing bytes. |
| `finish` | Supply the current revision. Rename the retained partial entry to its final path, then mark completed. Repeated finalization is idempotent. |

An exact retry of the last acknowledged write is accepted by comparing its
offset, size, revision and actual stored bytes. Changed/stale writes and sparse
gaps are rejected. Clients must serialize writes within one operation. Distinct
operations may be interleaved; service store access remains serialized.

Partial media and operation journals live inside the existing VFS project.
Ordinary project enumeration excludes stream journals and partial upload entries.
Neither a timeout nor an unknown action deletes acknowledged material. Completed
media becomes an ordinary project entry. No abort/delete operation is provided.
File close/flush errors fail the write rather than acknowledging durability.

This is not a transactional, crash-proof media recovery guarantee. A process or
power failure may leave a retained, unfinalized container that needs a later
format-specific repair tool. Inspecting an operation does not repair media.

`VfsChunkUploadSmoke` checks open-ended append, header patching, retry semantics,
destination protection, hidden partials, finalization and exact readable bytes,
alongside the existing large known-size upload and ranged-read checks.
