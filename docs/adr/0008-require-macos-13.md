# Require macOS 13 or later

Codex Muse-On v1 supports macOS 13 or later and uses `SMAppService.mainApp` for Start Automatically. Supporting older macOS releases would require a separate legacy login-helper path, increasing lifecycle and testing complexity for a small compatibility gain. The first release therefore uses the modern user-approved login-item model only.
