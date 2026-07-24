# Use a stable local install path

Before granting macOS permissions or testing Start Automatically, the signed local bundle is copied to `/Applications/Codex Muse-On.app`; the changing repository build bundle is never registered as the login item. Builds still originate inside the repository, and installation is a separate explicit user-approved live-test step. This reduces TCC and login-item churn caused by changing bundle paths while keeping local development artifacts isolated.
