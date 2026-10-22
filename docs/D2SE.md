# D2SE — metadados

Fonte: `D2SE.ini` (raiz) + `D2SE/CORES/*/D2SE_SETUP.ini` + `MODS/*/D2SE_SETUP.ini`.  
D2SE.exe = launcher Windows PE32 — **não portável**.

## `D2SE.ini` (raiz) — campos observados

| Campo | Secção | Valor exemplo | Suporte d2nx |
|-------|--------|---------------|--------------|
| `CreatedFor` | `[D2SE]` | `2.2.0` | leitura futura |
| `ActiveFolder` | `[USERSETTINGS]` | `D2SE_Eastern Sun R6E 4.1N` | modelo launcher (id de pasta) |
| `D2CMDLine` | `[USERSETTINGS]` | vazio | mapear flags nativas quando existirem (`-w` irrelevante) |
| `DDRAWID` | `[Protected]` | RainbowBug Vista/Win7 | **unsupported** (Windows DDraw) |
| `endoffile` | `[END]` | `4711` | anticorruptela D2SE — ignorar |

## `D2SE_SETUP.ini` — campos relevantes (Protected / USERSETTINGS)

| Campo | Significado | d2nx |
|-------|-------------|------|
| `D2Core` | Core obrigatório (`1.07`…`1.13c`) | **suportado** no preflight (aviso/bloqueio se simulação ≠ core) |
| `ModName` / `ModTitle` | Nome UI | suportado (catálogo) |
| `ModMajorVersion` / `ModMinorVersion` / `ModRevision` | Versão | suportado (display) |
| `ModDll1..3` | DLLs injectadas | **unsupported** como runtime; marca bloqueado |
| `ModAllowPlugY` / `EnablePlugY` | PlugY | bloqueado (código) |
| `Renderer` | 0 DDraw / 1 D3D / 3 Glide | **unsupported**; renderer nativo próprio |
| `WindowMode` | `-w` | n/a fullscreen Switch |
| `Language` | pasta Language_XXX | futuro |
| `NoSound` | `-ns` | futuro áudio |
| `EnableHC` | hardcore | futuro saves |
| `Direct` | direct/txt | futuro data overrides |
| `ModReadme` | ficheiro/URL | display only |
| `CustomMpq` / pastas MPQ | overlays | futuro merge MPQ |

## Cores presentes

`D2SE/CORES/`: 1.07, 1.08, 1.09b, 1.09d, 1.10f, 1.11b, 1.12a, 1.13c — cada um com DLLs PE32 próprias.

**Regra:** cores **não são intercambiáveis**. Um save/mod 1.13c não corre “em cima” de lógica 1.10f sem camada de compatibilidade explícita.
