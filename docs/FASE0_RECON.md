# FASE 0 — Reconhecimento e Inventário Técnico

**Data:** 21/10/2026  
**Status:** Concluída com sucesso

---

## 1. Inventário de Arquivos e Hashes (SHA-256)

### 1.1 Core 1.13c Vanilla Limpo (Origem: `/mnt/steam/Diablo II LoD 1.13c -D2SE/`)
Os binários originais da Blizzard foram validados a partir de `D2SE/CORES/1.13c` e da raiz (somente leitura):

| Arquivo | Tamanho (bytes) | Hash SHA-256 |
|---|---|---|
| `Game.exe` | 61.440 | `74fe9c092a521f7710392548c82f81544e531107db2358617e83818874db40a2` |
| `Diablo II.exe` | 36.864 | `147e33cc5d2dd9520d143847b35c1c8410a432c605155e0d9678b5b9450a7938` |
| `Storm.dll` | 372.736 | `4b5fcaf87c98676e6be94f980b06b893c50f1505343cbbf038c5b331a2da2d5b` |
| `Bnclient.dll` | 139.264 | `3631ca889dfa3cb149dd5b1722d3231851661b40514d4fbb4f00c529e76a56f5` |
| `D2Client.dll` | 1.093.632 | `dd8bc6025de921216a97c17f97cd1a50fbb85926e838ec60e13451448836d906` |
| `D2Common.dll` | 679.936 | `59fa5928522f566f2bf99675571206ad70df889c89d3d07fa87edf5083e06e10` |
| `D2Game.dll` | 1.138.688 | `24b293b86f95bea0228826c502ea04737ba21b91ca9832e52aac4b11fef14f53` |
| `D2CMP.dll` | 163.840 | `2ee205f484161c5ed854f30aed12565a7ac3e97fd1a838e697aaefe4dc349756` |
| `D2gfx.dll` | 77.824 | `50284647a28e8839028e8acb021a6ebe48c006e431139969ccd0fe126e6e8811` |
| `D2Win.dll` | 147.456 | `a9afb52d5116f77534f3e83eeb71e6d2d980a6b1c59be5dd38d503c2334730fb` |
| `D2sound.dll` | 98.304 | `b24dfb0159c76867e7f3d14602e73bccc37d59b7c8d2def88213b01084b61a72` |
| `D2Lang.dll` | 77.824 | `206386a81f4046c693f2aca60b9ee19fffd05b1157bbb60992c0d4caad9b67ec` |
| `D2Launch.dll` | 167.936 | `99cabc4751c95d035b5e77151b3605b809884fec2c10edf37d4c4e4e231e8479` |
| `D2Net.dll` | 49.152 | `99af2f04a87fdbd91810be963df9c365cef529ae22e6a345193ba5018e9910c9` |

### 1.2 Fog.dll (Comparativo)
- `Fog.dll.orig` (Vanilla 1.13c oficial Blizzard, 212.992 bytes): `96c05770ab06b71a7a2d6abe5dc7c2f943b30ac8fe367e44bdbad7e2643bb863`
- `Fog.dll.patched` (Vanilla com patch de CriticalSection Wine): `4a656de26ca60be03358fb144133dda6f2971ecdab1c615ada6f46be7475443d`
- `Fog.dll` (na raiz do D2): `388eecb60f71d96abb85e3c306e016823df72982a282725eec8ee45b4632a3b0`

### 1.3 Arquivos MPQ Base do LoD
| Arquivo | Tamanho (bytes) | Hash SHA-256 |
|---|---|---|
| `d2data.mpq` | 267.642.202 | `99e53768ddb87ca70b54529ff15043aaa71353cc250e757ae20e9c3d002ca431` |
| `d2exp.mpq` | 250.156.780 | `04e35bffeae8d9959042832055702f8b5b4567ca5505ff1df0cf3fd6d5beb6c2` |
| `d2char.mpq` | 266.908.965 | `653165341a64d3ace677aa527f56cf092e95159271848c603f34110ef579964c` |
| `d2sfx.mpq` | 51.948.991 | `7dc7dc41a889a20e376cc8059a874b0ea1bfb61cd6935d0a9bce9b85ccb832d4` |
| `d2speech.mpq` | 163.021.838 | `7a238f808be9fbc74da55c98dc629ab7fac9a4f40c537cec3c1b042fa45d211a` |
| `d2music.mpq` | 560.130.590 | `a2e228393d69afcb937b067d4ae4df8753aa044c99ad90ddb1d25996f119186f` |
| `d2video.mpq` | 8.018.715 | `a422c164ce408ec36a059ace80102e54781ffddc96ffc5649a90f4a5bbd264c6` |

### 1.4 D2PAD (Origem: `/mnt/steam/Diablo II LoD 1.13c -D2SE/D2Pad2/`)
| Arquivo | Tamanho (bytes) | Hash SHA-256 |
|---|---|---|
| `D2Pad.dll` | 102.400 | `ee1d77ed74d8e5819a61d648c3a0be7af537b65e690370fdd3db2fc7023b35be` |
| `D2Pad.ini` | 5.177 | `6c07c656c629872e80859be0fd51d0d0eb270d234c0a302afb0fb6e3dc575c61` |
| `D2Pad_median.ini` | 4.095 | `f59485570e5a422680adf652e98849a8fac84c329a8b9d283ff7fae7281626bb` |

### 1.5 MODs — Median XL Sigma 2.9.2 (Somente Leitura / Backup Proibido)
| Arquivo | Tamanho (bytes) | Hash SHA-256 |
|---|---|---|
| `D2Sigma.dll` | 7.989.568 | `899c1c1ef8118fee4f23b47c46891074bf218f36fe97232d4dd2a84cf85575fc` |
| `P113c.dll` | 108.544 | `a3527917b94ae379469807e8669d12aa1fb010e79602ea6f0496dd88ce62d9eb` |
| `glide3x.dll` | 2.804.224 | `1ef7b7449dfeba6c16e4756c72a7b211175596a21952beb3c5bd0b16384df717` |
| `d2gl.mpq` | 65.115.716 | `f6c85e6f0b77df3524dd7f4aafa66fd81ddf505907e363b531a6d47b0a7d201a` |
| `patch_d2.mpq` | 3.915.305 | `81470b762c497a8fcaf9a09990c0e869a4175395ef1fae13bd85cfec7b688ddc` |
| `MXL.mpq` | 552.299.345 | `a9b4d240a06c3cb0b8dec84650e29955055ab7e149f328b70c29a07ba4b9fb39` |
| `D2Pad.dll` | 102.400 | `ee1d77ed74d8e5819a61d648c3a0be7af537b65e690370fdd3db2fc7023b35be` |

---

## 2. Emulador Instalado no Host

- **Identificação:** Ryujinx 1.3.3 (`/usr/bin/ryujinx`).
- **Runtime:** .NET 10.0.4 em CachyOS (Linux x86_64).
- **Diretório de Dados:** Suporta flag `--root-data-dir <DIR>`, permitindo isolamento total dentro de `build/ryujinx-test/` sem tocar em `~/.config/Ryujinx` nem na biblioteca pessoal do usuário.
- **Suporte a Homebrew/NRO:** Total, via Homebrew Menu / Forwarder NCA (`build/ryujinx-test/hbl/ncas/nca-1.nca`).

---

## 3. Teste de Viabilidade do AUTORUN no Emulador

- **Resultado:** **Viável e comprovado por telemetria.**
- **Evidência instrumental (`build/ryujinx-test/sdcard/switch/wine/logs/autorun_runtime.log`):**
  - O runtime do Autorun (`wine-nx-runtime.nro`) inicializou o subsistema Wine + Box64.
  - Carregou os módulos x86: `glide3x.dll`, `D2Client.dll`, `D2Common.dll`, `D2Glide.dll`, `D2Sigma.dll`.
  - Processou mais de 2.290 frames via OpenGL/Glide (`gl_frames=2290`) sem falhas estruturais de memória.
- **Conclusão:** O emulador Ryujinx consegue simular satisfatoriamente o ambiente de execução do Autorun no Switch para testes de boot, renderização gráfica e carregamento de DLLs.

---

## 4. Canal Oficial de Obtenção do Median XL Σ 2.14.x

- **Versão Atual Oficial:** Median XL Σ **2.14.6** (identificada no manifesto público oficial).
- **Manifesto Oficial:**
  - URL: `https://launcher.median-xl.com/mxl/release/public/manifest`
  - Tag: `2.14.6`
  - Canal: `mxl_release`
  - Localização dos arquivos: `https://get.median-xl.com/get.php`
- **Validação de Download:**
  - Testado o download direto via API oficial: `curl -sL --get "https://get.median-xl.com/get.php" --data-urlencode "type=mxl_release" --data-urlencode "tag=2.14.6" --data-urlencode "file=medianxl-bG9jYWw.mpq"`
  - Hash MD5 verificado contra o manifesto (`3635d26109cd41d9bfa388196f973db7`): **100% idêntico**.
  - Não requer emulação de navegador nem bypass de Cloudflare, pois a API direta do launcher não possui bloqueio por Turnstile.

---

## 5. Plano de Testes e Próximos Passos (Fases 1 e 2)

1. **Fase 1 (Montagem Limpa no PC):**
   - Baixar os arquivos oficiais do Median XL 2.14.6 pelo endpoint da API oficial para `build/mxl214/`.
   - Montar o prefixo limpo com o Core 1.13c vanilla da Blizzard (Game.exe, Storm.dll, D2Client.dll etc.) + Fog.dll do mod + D2Sigma.dll + MPQs base + D2GL oficial do 2.14.6.
   - Testar inicialização direta via `Game.exe -w -3dfx` em Wine isolado com Xvfb (display virtual) sem janelas no desktop do usuário.
   - Criar personagem, testar entrada no jogo, save e load com screenshots.

2. **Fase 2 (D2PAD):**
   - Esclarecer com o usuário os detalhes de autoria/código-fonte do D2PAD e método de injeção in-process (sem D2SE).
   - Validar compatibilidade dos hooks de memória do D2PAD com o `D2Client.dll` e `D2Sigma.dll` da versão 2.14.6.
