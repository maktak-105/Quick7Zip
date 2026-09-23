# Code signing policy

Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org).

Status: the application to SignPath Foundation is under review. Releases published before approval are not signed.

## What is signed

- Only Windows executables built from the source code in this repository by the GitHub Actions release workflow (`.github/workflows/release.yml`) and published on [GitHub Releases](https://github.com/maktak-105/Quick7Zip/releases).
- Third-party binaries shipped in the package (for example `WebView2Loader.dll` or runtime libraries) are not built by this project and are not signed by it.
- Every signing request is approved manually by an approver.

## Team roles

- Committers and reviewers: [maktak-105](https://github.com/maktak-105)
- Approvers: [maktak-105](https://github.com/maktak-105)

All team members use multi-factor authentication for GitHub and SignPath.

## Privacy policy

This program will not transfer any information to other networked systems unless specifically requested by the user or the person installing or operating it.

## System changes

Quick7Zip can add a "Quick7Zip" entry to the Explorer context menu for the current user (HKEY_CURRENT_USER). On first start it asks the user whether to add the entry and stores the answer. It can be turned on or off later in the Settings; turning it off removes the entry.
