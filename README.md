# Deep Spark

Deep Spark is a Hugo + Blowfish site for **real-time rendering** writing: articles, experiment logs, and profiling/analysis workflows.

## What’s inside

- **Posts**: longer, polished write-ups (pipeline design, techniques, trade-offs)
- **Experiments**: short iterative logs (what I tried today + next steps)
- **Analysis**: repeatable workflows (profiling, frame capture, performance interpretation)

## Folder map (where to edit things)

- Site-wide config: `config.toml`
- Blowfish overrides: `config/_default/`
	- Theme params: `config/_default/params.toml`
	- Menus: `config/_default/menus.en.toml`
	- Author + site description: `config/_default/languages.en.toml`
- Content:
	- Home page: `content/_index.md`
	- About page: `content/about.md`
	- Posts: `content/posts/`
	- Experiments: `content/experiments/`
	- Analysis: `content/analysis/`
- Static assets (served as-is): `static/`
	- Images: `static/images/` → available at `/images/...`

## Prerequisites

- **Hugo Extended** (required by Blowfish)
- Git

Check Hugo:

```powershell
hugo version
```

## Run locally (PC)

```powershell
# from repo root
hugo server
```

Open:

- http://localhost:1313/

## Open on your phone (same Wi‑Fi)

1) Find your PC’s IPv4 address (example: `192.168.100.12`):

```powershell
Get-NetIPAddress -AddressFamily IPv4 |
	Where-Object { $_.IPAddress -match '^192\.|^10\.|^172\.' } |
	Format-Table IPAddress,InterfaceAlias
```

2) Start Hugo bound to your network interface:

```powershell
hugo server --bind 0.0.0.0 --port 1313 --baseURL http://YOUR_PC_IP:1313/ --appendPort=false
```

3) On your phone open:

- `http://YOUR_PC_IP:1313/`

If it doesn’t load, allow inbound port 1313 (run once):

```powershell
New-NetFirewallRule -DisplayName "Hugo Dev Server 1313" -Direction Inbound -Protocol TCP -LocalPort 1313 -Action Allow
```

## Writing workflow

- Create a new post folder under `content/posts/<slug>/index.md`
- Drop images under `static/images/` and reference them as `/images/<file>`

Suggested structure for technical posts:

1. Goal
2. Constraints (platform, frame budget)
3. Approach (algorithm/data)
4. Debug views (how you validate correctness)
5. Performance (timings + why)
6. Results (screenshots/videos)
7. Next steps

## Theme

This repo uses the **Blowfish** theme as a git submodule.

If you cloned without submodules:

```powershell
git submodule update --init --recursive
```

## Deploy to GitHub Pages (automatic)

This repo contains a GitHub Actions workflow:

- `.github/workflows/deploy-pages.yml`

To enable publishing:

1) GitHub repo → **Settings → Pages**
2) Source: **GitHub Actions**

After that, every push to `main` builds and deploys.

## Credits

- Hugo: https://gohugo.io/
- Blowfish theme: https://github.com/nunocoracao/blowfish
