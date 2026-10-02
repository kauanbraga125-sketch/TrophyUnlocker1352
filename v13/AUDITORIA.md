# Auditoria da continuação V12 → V13

Data: 2026-10-02. Referência recebida: `Trophy_Unlocker_13.52_V12_UI(1).pkg`, SHA-256 `ba6d7cc54d7148d89135ace0fbe7bd2fa1fc42557be498754971a03a80b51f55`.

Base de código: commit `19cc8b7555ea27e0b20efc9743728da5d3621794` do repositório `kauanbraga125-sketch/TrophyUnlocker1352`. As strings da interface e o conteúdo extraído do PKG são consistentes com `v12/main.cpp` e `ci/build_v12.sh`. A V12 foi relatada pelo usuário como capaz de abrir; não foi executada aqui em hardware.

## Achados confirmados

- O PKG V12 recebido passa nas 28 checagens de integridade e assinatura de `PkgTool.Core pkg_validate`. Isso não testa execução ou permissões em firmware.
- A V12 só inicia a busca com TRIÂNGULO. Enumera `/user/app` e `/mnt/ext0/user/app`; `/user/appmeta` é apenas sondado, não usado como fonte de entradas.
- A V12 usa `opendir/readdir` e não chama `sys_sdk_jailbreak`. Um processo que continue isolado pode não enxergar esses caminhos mesmo com jogos instalados.
- A V12 mostra apenas CUSA e capas desenhadas; não implementa seleção manual, extração do título via SFO ou desbloqueio de troféus.
- O histórico recuperado registra `-2/ENOENT` em sondas anteriores. A hipótese de isolamento é compatível com esse resultado, mas não é provada somente por ele. A V13 registra o retorno da API e dos caminhos para distinguir os casos.

## Mudanças aplicadas

- Interface SDL2, renderizador de software e fonte da V12 mantidos. Mesmos CATEGORY `gd`, ATTRIBUTE `0`, PAID `0x3800000000000011`, CONTENT_ID, TITLE_ID e módulos básicos. APP_VER/VERSION passam a `01.01`.
- Busca automática por etapas: uma pasta ou os metadados de um jogo por quadro. Mais fontes: ext1, appmeta, appmeta/external e system_data/priv/appmeta. Limites de entradas e deduplicação por CUSA.
- Leitura nativa por `sceKernelOpen/Getdents/Read`, com retorno SCE normalizado e parser limitado do formato FreeBSD do PS4. SFO/imagens são lidos até EOF com limite, sem usar o tamanho de `struct stat`.
- Botão TRIÂNGULO da biblioteca usa o protocolo 1.00 da API pública do GoldHEN, comandos VERSION e JAILBREAK, sem offsets de firmware. A estrutura de backup tem tamanho conferido em compilação. Resposta desconhecida não dispara o comando de jailbreak.
- Navegador de pastas, entrada de CUSA pelo controle, persistência da seleção por arquivo temporário + fsync + rename. Somente arquivos próprios do app em `/data/TrophyUnlocker1352` são gravados.
- Título SFO com limites/consistência e texto UTF-8. Capas PNG com limite de arquivo, dimensões máximas e no máximo três texturas ativas; imagem substituta em caso de erro.
- Captura de conexão/desconexão do controle, cache de textos e estados explícitos para metadados versus pacote realmente aberto. O retorno normal de `main()` continua fora do fluxo de produção.

## Verificações locais

O teste de catálogo cobre diretórios internos/externos, deduplicação, campos e limites SFO, registros de diretório truncados, 5.000 entradas malformadas geradas deterministicamente, busca manual por pasta/CUSA, persistência, escrita negada e remoção de metadados entre buscas. Passou com AddressSanitizer e UndefinedBehaviorSanitizer. LeakSanitizer foi desativado no ambiente local por incompatibilidade com a inspeção de processos do sandbox; não se afirma ausência de vazamentos em hardware.

As seis telas foram renderizadas a partir de `v13/main.cpp` em SDL2 no computador: biblioteca populada, biblioteca vazia, pastas, CUSA, detalhes e diagnóstico. Isso verifica a disposição visual e o código de renderização, não o driver de vídeo/controle do PS4.

O PKG V13 foi compilado por LLVM 18/OpenOrbis v0.5.4 e passou nas 28 checagens do PkgTool. O `eboot.bin` extraído corresponde ao build. `libc.prx`, `libSceFios2.prx`, `right.sprx` e `VeraMono.ttf` são idênticos aos extraídos da V12 enviada. O hash exato do PKG entregue fica em `dist_v13/SHA256SUMS.txt`.

## Limites e continuação

Sem execução em PS4 nesta sessão. Ainda dependem do console: abertura da V13, disponibilidade do serviço GoldHEN no payload do usuário, visibilidade efetiva dos pontos de montagem, carregamento de capas reais e persistência após reabrir. A busca manual não concede acesso a diretórios bloqueados e uma entrada de appmeta pode ser residual. A biblioteca está limitada a 512 CUSAs, cada leitura de diretório a 4.096 entradas, SFO/lista manual a 64 KiB e capas a 4 MiB/2048×2048.

Não houve edição de banco de troféus, sincronização PSN, carregamento do plugin V5 ou promessa de desbloqueio nesta build. A referência desta continuação é a UI V12 funcional, não a implementação antiga V4/V5.

## Fontes primárias usadas

- [Código e build V12 do próprio projeto](https://github.com/kauanbraga125-sketch/TrophyUnlocker1352/tree/19cc8b7555ea27e0b20efc9743728da5d3621794/v12).
- [GoldHEN Plugins SDK, commit bcea3c7](https://github.com/GoldHEN/GoldHEN_Plugins_SDK/tree/bcea3c7ef01dac6d7f9f49ebf9e90fe66d86f5f7): `include/GoldHEN.h`, `source/GoldHEN.c` e `source/Syscall.c`, API e ABI usadas pelo adaptador.
- [OpenOrbis v0.5.4](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/tag/v0.5.4): sample SDL2, importações libkernel, formato dirent, toolchain e empacotamento.
- [orbis-compat](https://github.com/orbis-ports/orbis-compat): evidência pública dos problemas de layout de stat/retorno de main no toolchain. Consultado como referência; nenhuma atualização de SDK nem dependência desse projeto foi introduzida na V13.
