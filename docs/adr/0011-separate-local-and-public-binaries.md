# Separate local and public binary delivery

This phase produces only the owner's local test bundle. The user-approved
stable local development signing identity is `Codex Muse-On Local Development`;
the packaging flow uses it when available and falls back to ad-hoc signing when
it is unavailable. This local identity is not Developer ID signing and does not
authorize distribution. Public binary distribution remains blocked until a
later phase provides Developer ID signing and Apple notarization; source
preparation and publication are separate work. This avoids asking other users
to grant broad macOS input permissions to binaries whose signing identity is
not suitable for public distribution.
