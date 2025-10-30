# SKYM – SSH Key Manager for Windows

**SKYM** is a Windows SSH agent and key manager that provides secure, user-friendly management of SSH private keys with advanced features like confirmation prompts, destination constraints, and seamless integration with popular SSH clients.

## ✨ Key Features

- 🔐 **Secure Key Storage** – Private keys stay in memory, never written to external processes
- 💬 **Confirmation Dialogs** – Optional user prompts before signing with optional "remember" durations
- 🎯 **Destination Constraints** – Restrict keys to specific hosts/principals for enhanced security
- 🔌 **Universal Compatibility** – Works with PuTTY, Windows OpenSSH, WSL2, and Hyper-V guests
- 🔑 **KeePass Integration** – Seamless integration with KeePass2 (via KeeAgent) and KeePassXC
- 🔔 **Smart Notifications** – Get notified about key usage, operations, and declined requests
- 🖥️ **System Tray** – Convenient access to key list and settings via tray icon

## 🚀 Quick Start

### Installation

1. Download the latest release from [Releases](http://github.com/michal-pod/skym/releases)
2. Verify GPG signature (optional but recommended). All releases are signed using this key (GPG Key ID: B5201C42AAD6DB2CF32F92B008497C69E88074C6)
3. Run the installer or extract the portable version
4. Launch SKYM – a system tray icon will appear

### Loading Keys

SKYM works with existing SSH keys loaded via:
- **ssh-add** (Windows OpenSSH)
- **KeePass2** with [KeeAgent plugin](https://lechnology.com/software/keeagent/)
- **KeePassXC** (built-in SSH agent support)

### Basic Usage

1. Load your SSH private key using one of the methods above
2. Connect with any SSH client (PuTTY, OpenSSH, etc.)
3. If confirmation is required, approve the signing request in the SKYM dialog
4. Optionally select "Don't ask again for..." to remember your choice

## 🔧 Compatibility

### Supported Clients

| Client | Support | Notes |
|--------|---------|-------|
| **PuTTY / Pageant** | ✅ Full | Enable Pageant compatibility in Settings |
| **Windows OpenSSH** | ✅ Full | Enable named-pipe support in Settings |
| **WSL2** | ✅ Full | Use `skym-npp` helper with socat or skym-ga|
| **Hyper-V VMs** | ✅ Full | Configure access in Settings → Hyper-V, then enable guest integration |

### Supported Key Managers
| Key Manager  | Support | Notes |
|--------------|---------|-------|
| **KeePass2** | ✅ Full | Configure KeeAgent to Client mode |
| **KeePassXC** | ✅ Full | Enable SSH Agent in Settings |

### Platform Requirements

- Windows 10/11 (x64)
- MSVC 2019+ Redistributable [x64](https://aka.ms/vs/17/release/vc_redist.x64.exe) [arm64](https://aka.ms/vs/17/release/vc_redist.arm64.exe) installed

## 🎛️ Advanced Features

### Destination Constraints

Restrict key usage to specific destinations using the `skym-dcc` tool:

```bash
# Generate constraints file
skym-dcc known_hosts constraints.cdc "awesome_host" "boring_host>cool_user@avesome_host"

# Load into SKYM via GUI
# Key List → Select Key → Load Constraints
```

**Note:** PuTTY does not support destination constraints; restricted keys will be ignored by PuTTY clients.

### WSL2 Integration

Add to your `~/.bashrc` or `~/.zshrc`:

```bash
if grep -qEi "(Microsoft|WSL)" /proc/version &> /dev/null ; then
    export SSH_AUTH_SOCK=$HOME/.ssh/agent.sock
    ss -a | grep -q $SSH_AUTH_SOCK
    if [ $? -ne 0 ]; then
        rm -f $SSH_AUTH_SOCK
        (setsid nohup socat UNIX-LISTEN:$SSH_AUTH_SOCK,fork EXEC:/path/to/skym-npp.exe >/dev/null 2>&1 &)
    fi
fi
```

### Hyper-V Guest Access

Inside Hyper-V guest (Linux):

```bash
export SSH_AUTH_SOCK=$HOME/.ssh/agent.sock
socat UNIX-LISTEN:$SSH_AUTH_SOCK,fork VSOCK-CONNECT:2:11888
```

## 📖 Documentation

For detailed documentation, visit the [SKYM Documentation](https://github.com/michal-pod/skym-documentation) repository.

## 🛡️ Security Model

- Private keys are stored in the agent process memory
- Keys are never written to disk by the agent
- Destination constraints evaluated before cryptographic operations
- When locked all keys are encrypted in memory
- UI-level enforcement of confirmation rules and timeouts

## 📄 License

SKYM is licensed under the GNU General Public License v3.0 or later. See [LICENSE](LICENSE) for details.

## 🙏 Acknowledgments

- Compatible with the SSH agent protocol
- Pageant compatibility for PuTTY ecosystem
- Windows OpenSSH named-pipe integration

---

**Need Help?** Open an [issue](https://github.com/michal-pod/skym/issues) or check the [documentation](https://github.com/michal-pod/skym-documentation).
