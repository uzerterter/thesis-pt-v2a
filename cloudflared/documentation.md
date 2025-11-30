# Cloudflared Tunnel + Companion Client Setup

## 1. Create the Cloudflare Access application

1. In Cloudflare Zero Trust, go to **Access → Tunnels** and select the tunnel you created (e.g. `ludwig-thesis_apis`).
2. Under **Public Hostnames**, add both hostnames you want (e.g. `mmaaudio.linwig.de`, `hyvf.linwig.de`) and point them to the respective services (`http://mmaaudio-api:8000`, `http://hunyuanvideo-foley-api:8001`).
3. In Cloudflare Zero Trust → **Access → Applications**, create a new **Self-hosted** application and include both hostnames. Choose **Service Token** as the authentication method.
4. Cloudflare will generate a **Client ID** and **Client Secret**. Copy these values now (you can regenerate later if needed).

## 2. Update the companion client configuration

The companion client reads `companion/api/config.json` (ignored by git). Create it by copying from the sample:

```bash
cp companion/api/config.sample.json companion/api/config.json
```

Edit `companion/api/config.json` and set:

```json
{
  "use_cloudflared": true,
  "api_url_direct": "http://localhost:8000",
  "api_url_cloudflared": "https://mmaaudio.linwig.de",
  "cf_access_client_id": "<your-client-id>",
  "cf_access_client_secret": "<your-client-secret>"
}
```

- `use_cloudflared: true` tells the client to hit the tunnel URL and attach the access headers.
- To switch back to local access, set `use_cloudflared: false`. The client will automatically use `api_url_direct` and skip the headers.
- If you have multiple hostnames, you can either run the CLI with `--api-url` per call, or change `api_url_cloudflared` to the desired host.

## 3. How the companion client uses the config

- `companion/api/config.py` loads `config.json` if present (otherwise it falls back to the defaults in `config.sample.json`).
- `use_cloudflared()` checks whether Cloudflare mode is enabled and a tunnel URL is defined.
- `get_api_url()` returns the active URL (Cloudflare or direct).
- `get_cf_headers()` returns the `CF-Access-Client-Id` / `CF-Access-Client-Secret` headers only when Cloudflare mode is enabled and both values are set.

## 4. Companion CLI usage

No changes are required in CLI invocations:
- When `use_cloudflared` is on, requests are routed through Cloudflare and include the access headers.
- When off, the CLI behaves as before.

If you need to temporarily override the URL without editing the config, you can still pass `--api-url` on the CLI.

## 5. Rotating or revoking the token

- In Cloudflare Zero Trust → Access → Service Tokens you can view, revoke, or regenerate the token.
- After rotating, update `companion/api/config.json` with the new values and restart any running CLI sessions.

## 6. Troubleshooting

- If `check_api_health` fails, verify:
  - The tunnel is running (`docker compose logs cloudflared`).
  - DNS records in Cloudflare point to the tunnel.
  - The service token in `config.json` matches the one shown in Cloudflare Access.
- Use `curl -H "CF-Access-Client-Id: …" -H "CF-Access-Client-Secret: …" https://mmaaudio.linwig.de/` to confirm the headers work manually.
- To debug config loading, temporarily add `print(get_config())` inside `companion/api/config.py` or run the CLI with `python -m companion.api.config`.
