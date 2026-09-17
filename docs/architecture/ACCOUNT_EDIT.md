# Account editing

The account card shows `childUsername`, `username`, and a notice that existing passwords cannot be retrieved. Edit opens a modal dialog with a child-name field, a derived read-only username, and an optional new password with Show/Hide. Delete retains its confirmation flow.

`PUT /api/v1/child/{id}` requires ADMIN and verifies ownership by principal ID. The request contains `childUsername` and optionally `newPassword`; null or empty password preserves the credential. The full username remains owner username + trimmed childUsername, limited to 50 characters. Duplicate names return 409, foreign ownership 403, missing accounts 404, and invalid input 400. Responses never contain passwords.

Newly reset child passwords use salted PBKDF2-HMAC-SHA256 (600,000 iterations, 256-bit output). Child login supports this format and existing plaintext credentials. This change does not migrate existing credentials or change admin credential storage. Update client and management API together.

Renaming changes the next login username. Updating an account does not revoke existing JWT/relay sessions; existing sessions retain their original token username until login again. Password reset here changes future login credentials, not an emergency session-revocation operation.

Tests cover ownership/role checks, invalid and duplicate names, retaining passwords, reset-and-login, password-free responses, widget signals, masking, and clearing the new-password field on close.

## Vietnamese client and account page states

The client UI uses concise Vietnamese labels. Technical identifiers and user data are unchanged. HTTP error codes are mapped to Vietnamese messages in `ApiClient`; raw server errors are not shown as UI copy. Common Qt input-menu translations are bundled in `Resources/translations/qt_vi.ts` and `qt_vi.qm`; regenerate with `lrelease` when changing the TS source.

Account widgets live in `GUI/Components/Accounts`: `AccountCardWidget`, `CreateAccountDialog`, `EditAccountDialog`, and shared `PasswordField`. Widgets emit intents; `AccountController` coordinates the HTTP service and store. Forms do not perform network calls.

The page has loading, empty, populated, no-results, and retryable load-error states. The empty state has exactly one add action. The count card remains separate and appears with loaded accounts. Search matches both account name and login name. Failed refreshes retain cached accounts. Add/edit prevent duplicate submission and close only after success; deleting requires confirmation and blocks repeated actions while pending. Successful mutations refresh the list without a success popup.

The unfinished global toolbar and other placeholder controls remain visible but disabled with “Sắp có”. Session/log/settings backends remain outside this change; their existing sample data has not been replaced with a live implementation.
