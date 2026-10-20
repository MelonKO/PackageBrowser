# Third Party Notices

This document lists third-party software and libraries that Package Browser uses at build time or at runtime, together with their licenses and required notices.

## toml++

Used at build time to parse plugin manifests (`plugin.toml`). Injected via `FetchContent` from https://github.com/marzer/tomlplusplus (tag `v3.4.0`). Licensed under the MIT License.

> MIT License
>
> Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
>
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
> documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
> rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
> permit persons to whom the Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
> Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
> WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
> COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
> OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Qt 6

The application links against Qt 6 (Core, Widgets, SQL). Qt is a third-party product distributed under the relevant Qt licensing terms; when distributing this application you must also comply with the Qt license requirements for the Qt version you ship. See the Qt licensing documentation (https://www.qt.io/licensing/).