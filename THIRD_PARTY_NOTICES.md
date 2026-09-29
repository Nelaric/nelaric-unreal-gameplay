<!-- Copyright (c) 2026 Nelaric -->

# Third-Party Notices

The MIT terms in LICENSE apply to material authored by Nelaric. They do not replace
licenses and notices for third-party software included in or installed for
this project.

PuerTS: The Unreal plugin source in NelaricGameplay/Plugins/Puerts comes from
Tencent/puerts, tag Unreal_v1.0.9. Tencent's PuerTS code is licensed under
the BSD 3-Clause terms in NelaricGameplay/Plugins/Puerts/LICENSE. That file
also identifies doT as an MIT-licensed third-party component. The plugin
additionally contains the following dependencies, whose own terms apply
under NelaricGameplay/Plugins/Puerts/ThirdParty:

- Standalone Asio headers in Include/asio: Boost Software License 1.0,
  reproduced in ThirdPartyLicenses/Asio-LICENSE_1_0.txt.
- WebSocket++ headers in Include/websocketpp: notices and license terms in
  ThirdPartyLicenses/WebSocketPP-COPYING and the source headers.
- libffi headers and libraries in Include/ffi and Library/ffi: license in
  ThirdPartyLicenses/libffi-LICENSE and copyright notices in the headers.

V8: The checked-in project enables PuerTS with the V8 9.4.146.24 backend
from the official PuerTS Unreal_v1.0.9 release. The V8 headers and
Windows, Linux, and macOS libraries are stored under
NelaricGameplay/Plugins/Puerts/ThirdParty/v8_9.4.146.24. The large
Lib/Win64MD/wee8.lib file uses Git LFS. The V8 license is retained in
Setup/V8-LICENSE and in the backend directory as LICENSE. It identifies
further components with their own terms.
QuickJS: The checked-in PuerTS QuickJS backend under
NelaricGameplay/Plugins/Puerts/ThirdParty/quickjs comes from the official
Unreal_v1.0.9 release. Its QuickJS license is retained in that directory
as LICENSE.

Node.js: The checked-in PuerTS Node.js 16.16.0 backend under
NelaricGameplay/Plugins/Puerts/ThirdParty/nodejs_16 comes from the same
release. Its license and bundled third-party notices are retained in that
directory as LICENSE. Large runtime binaries use Git LFS.

The Setup scripts also install TypeScript into a local, Git-ignored
node_modules directory using npm. An external Node.js executable and npm
are setup prerequisites; neither executable is bundled with this repository.
Any redistribution of a package containing these or other dependencies must
retain the notices and license texts required by those components.
