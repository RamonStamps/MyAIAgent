Title: Add DOM snapshot test hook + webview DOM test

Summary
- Adds a DOM snapshot hook to the extension webview so tests can request the rendered HTML safely.
- Adds a test command `myaagent.getWebviewSnapshot` that requests the snapshot and returns it to tests.
- Adds a Mocha integration test `test/suite/webview.dom.test.js` asserting key UI elements are present.
- Stores DOM snapshot messages in the extension `outputBuffer` to aid tests and debugging.

Files changed / added
- vscode-extension/extension.js — add snapshot handler and test command
- vscode-extension/test/suite/webview.dom.test.js — new integration test
- vscode-extension/package.json — add `test:webview` script

Testing
1. From `vscode-extension`, install deps and run integration tests:

```powershell
cd vscode-extension
npm install
node ./test/runIntegrationTest.js
```

2. To run only the webview DOM-focused tests:

```powershell
cd vscode-extension
npm run test:webview
```

Notes
- Branch: `feature/tests-dom-snapshot` (created locally).
- This PR is safe: snapshot only returns HTML from the webview; no secrets are exposed.
- CI: ensure the integration runner environment provides `@vscode/test-electron` and the test stub (or real agent) as configured in `test/runIntegrationTest.js`.

Next steps
- Add remote and push branch; example:

```powershell
git remote add origin <your-remote-url>
git push -u origin feature/tests-dom-snapshot
```

- If you want, I can add Playwright E2E scaffolding and a GitHub Actions PR workflow in a follow-up commit.

Reviewer notes
- Focus review on `extension.js` webview messaging and test runner integration. The DOM snapshot is intentionally minimal (outerHTML) to keep tests simple and stable.
