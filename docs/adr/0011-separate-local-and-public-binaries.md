# Separate local and public binary delivery

This phase produces only an ad-hoc-signed local test bundle for validation on the owner's Mac. Public binary distribution remains blocked until a later phase provides stable Developer ID signing and Apple notarization; source preparation and publication are separate work. This avoids asking other users to grant broad macOS input permissions to binaries whose signing identity changes between builds.
