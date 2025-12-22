# LightThreads

A Hugo + Blowfish site for real-time rendering articles, screenshots, and experiment logs.

## The one rule (so you never get lost)

- Main Hugo config lives in `config.toml`
- All Blowfish overrides live in `config/_default/`

If you see other `*.toml` configs outside those locations, they should NOT exist (they cause confusing overrides).

## Where things live (folder locations)

- Site config (main): `config.toml`
- Theme overrides (Blowfish): `config/_default/`
  - Theme params: `config/_default/params.toml`
  - Menus: `config/_default/menus.en.toml`
  - Language metadata + author profile: `config/_default/languages.en.toml`
- Blog posts: `content/posts/`
- Experiments: `content/experiments/`
- Static images/videos you link to: `static/`
  - Example: images in `static/images/` are served at `/images/...`

## Run locally (PC)

From the repo root folder:

```powershell
hugo server
```

Open on your PC:

- http://localhost:1313/

## Open the site on your phone (same Wi‑Fi)

1) Find your PC’s local IPv4 address (example: `192.168.0.50`):

```powershell
Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -match '^192\\.|^10\\.|^172\\.' } | Format-Table IPAddress,InterfaceAlias
```

2) Start Hugo bound to all network interfaces (from the repo root folder):

```powershell
hugo server --bind 0.0.0.0 --baseURL http://YOUR_PC_IP:1313/ --appendPort=false
```

3) On your phone (same Wi‑Fi), open:

- `http://YOUR_PC_IP:1313/`

If it doesn’t load, allow port 1313 through Windows Firewall:

```powershell
New-NetFirewallRule -DisplayName "Hugo Dev Server 1313" -Direction Inbound -Protocol TCP -LocalPort 1313 -Action Allow
```

## Deploy to GitHub Pages (automatic)

Workflow file location:

- `.github/workflows/deploy-pages.yml`

After you push to the `main` branch, GitHub Actions builds and publishes automatically.

Repo settings (GitHub UI):

- Repo → **Settings** → **Pages** → Source: **GitHub Actions**

## Custom domain ("LightThreads" URL)

To use a custom domain like `lightthreads.dev` or `lightthreads.com`, you need to:

- Buy/register the domain
- Configure GitHub Pages custom domain (Repo → Settings → Pages)
- Point DNS records to GitHub Pages

Tell me the exact domain you own/want (e.g. `lightthreads.dev`) and I’ll add the correct `static/CNAME` file and DNS instructions.
