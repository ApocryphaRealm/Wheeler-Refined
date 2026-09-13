# Wheeler-Refined - changelog

Written as changes happen, not reconstructed afterwards (rule 61). Each version carries its
**version-ledger status**, so this file cannot quietly claim more than the ledger does:

* **working** - observed running in game
* **untested** - built and packaged, not yet confirmed
* **failed** - built but crashed or malfunctioned; the number was reclaimed
* **scratch** - a hypothesis-test build that never held a real number

## 1.0.1 - 2026-09-12 - untested

### Added
- The settings page is reachable from Apocrypha Menu Framework's Mod Control Panel: an entry 'Wheeler - Refined / Settings' registers through AMF_RegisterPage when the framework is loaded, and its button arms a launch that opens Wheeler's own page once the framework menu closes. No framework header is vendored; the boundary is GetProcAddress, and registration is refused unless every export resolves.

### Changed
- dMenu is no longer referenced anywhere: the menu-open checks that kept the wheel up during live editing, the ammo wheel's blocking list, the input spy's menu list and the edit-mode 'Settings' hint now use Wheeler's own settings page and its SettingsPageKey binding. The dmenu_updateSettings mod-callback listener is kept so an INI written by any external tool still reloads live.