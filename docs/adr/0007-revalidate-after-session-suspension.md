# Revalidate after session suspension

Sleep, screen lock, and user-session deactivation immediately make Control Inactive and trigger hold release plus verified Pass-through restoration. After wake, unlock, or session reactivation, the app revalidates device pairing, permissions, filtering, and foreground state before it may resume automatically. This chooses a visible safety transition over preserving controller state across a suspended macOS session.
