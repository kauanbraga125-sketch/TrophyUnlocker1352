# Trophy Unlocker 13.52 — V13.1 Acesso

Correção pontual da V13 que o usuário confirmou abrir e navegar no PS4 13.52. O diagnóstico da V13 mostrou `SDK=-256`, acesso não executado e todas as pastas com `-2`. A V13.1 preserva o retorno bruto do SDK e distingue o comando de acesso não executado de uma resposta real. **Ainda é necessário confirmar no console se essa correção torna os jogos visíveis. O desbloqueio de troféus não está integrado nesta interface.**

## Instalar e usar

1. Com o GoldHEN ativo, instale `Trophy_Unlocker_13.52_V13_1_Acesso.pkg` e abra **Trophy Unlocker 13.52 V13.1 Acesso**. O pacote mantém o identificador anterior (`BREW13533`) com versão `01.02`.
2. A biblioteca procura automaticamente nos locais internos, externos e de metadados. Nomes e capas são carregados quando os arquivos correspondentes estão acessíveis.
3. Se continuar vazia, aperte **TRIÂNGULO na biblioteca**. Isso solicita acesso ao sistema de arquivos pela API pública do GoldHEN e repete a busca. A chamada acontece depois da interface estar aberta; não é executada automaticamente na inicialização.
4. Se não encontrar o jogo, **QUADRADO** abre a busca manual.
5. Para conferir os resultados, **OPTIONS** abre o diagnóstico. Uma foto dessa tela é suficiente para a próxima análise se o log não puder ser salvo.

## Busca manual por pasta

Escolha um local com CIMA/BAIXO e entre com X. Navegue até a pasta do jogo e aperte QUADRADO **quando estiver dentro dela**. O app aceita uma pasta `CUSA12345` ou uma pasta com `param.sfo` / `sce_sys/param.sfo` contendo o identificador do jogo.

- X: entrar na pasta selecionada.
- QUADRADO: adicionar a pasta atualmente aberta.
- O: subir uma pasta; na lista de locais, voltar à biblioteca.
- L1: voltar aos atalhos de locais.
- TRIÂNGULO: informar um CUSA, mesmo se a pasta não estiver acessível.

Os atalhos incluem armazenamento interno, externo, `/mnt/usb0`, `/mnt/usb1`, `/data` e a raiz `/`. Outros pontos de montagem podem ser alcançados navegando pela raiz. Selecionar uma pasta é uma operação de leitura; não move arquivos e não instala jogos.

## Informar CUSA pelo controle

Na busca manual, aperte TRIÂNGULO. ESQUERDA/DIREITA escolhe um dos cinco números; CIMA/BAIXO muda o número; X adiciona e O cancela. Exemplo de formato: `CUSA12345`.

O app procura os dados desse CUSA nos locais conhecidos. Se não conseguir verificar os arquivos, exibe **CUSA manual — acesso não confirmado**. Isso permite manter a seleção, mas não equivale a encontrar os arquivos ou a liberar troféus.

As escolhas são salvas em `/data/TrophyUnlocker1352/manual-games.txt`. Se não houver permissão de escrita, permanecem válidas apenas durante a sessão e a tela informa a falha.

## Biblioteca e detalhes

ESQUERDA/DIREITA navega; X abre detalhes; O volta; R1 atualiza a lista. O app distingue:

| Estado | O que foi verificado |
| --- | --- |
| Pacote do jogo acessível | Foi possível abrir `app.pkg` para leitura. |
| Dados do jogo encontrados | Foram encontrados metadados; podem existir mesmo após remover o jogo. |
| Pasta encontrada — pacote não confirmado | A pasta foi listada, mas o pacote não foi confirmado. |
| CUSA manual — acesso não confirmado | O usuário informou o identificador; os arquivos não foram verificados. |

As imagens de troféu são usadas quando não há capa legível. Não são jogos demonstrativos: o PKG final não inclui a biblioteca fictícia usada nos testes de tela.

## Se a lista continuar vazia

Na biblioteca, aperte TRIÂNGULO, aguarde a busca e abra OPTIONS. Envie uma foto do diagnóstico, a versão completa do GoldHEN e um CUSA que deveria aparecer. O log, quando gravável, fica em:

`/data/TrophyUnlocker1352/v13-diagnostic.txt`

`-2` significa caminho ausente ou não visível no ambiente do aplicativo; sozinho, não prova que o jogo não esteja instalado. `-13` indica acesso negado. `0` indica que a leitura da pasta funcionou. O retorno do GoldHEN e o acesso real às pastas são mostrados separadamente.

Na V13.1, `SDK bruto=256` identifica o protocolo público esperado. `CF` registra separadamente um sinalizador retornado pela chamada. A linha **Acesso: comando enviado** confirma que houve uma tentativa real. Um retorno bruto zero dessa chamada não substitui a verificação das pastas: o resultado da busca continua sendo a evidência de acesso aos jogos. Se aparecer **comando ainda não enviado**, envie a foto com o SDK bruto e CF; a V13.1 não inventa mais `-78` para esse caso.

## Compilar

Em Ubuntu 24.04: `bash ci/build_v13.sh`. O script fixa OpenOrbis v0.5.4 e verifica o SHA-256 do arquivo do toolchain. Não substitui o código por um ZIP antigo. Saídas em `dist_v13/`.

Com toolchain e ambiente do PkgTool já preparados, use `bash v13/build.sh`. Testes: `bash v13/test.sh`. Prévias de tela no computador: `bash v13/preview.sh` com SDL2, SDL2_image, FreeType e pkg-config instalados. As prévias usam arquivos fictícios somente no executável de teste.

## Próxima integração

Depois de confirmar no console a descoberta e os caminhos reais de pelo menos um jogo, a UI poderá receber a leitura do conjunto de troféus e a integração de desbloqueio local com o contexto nativo. Esta entrega não apresenta um botão de desbloqueio sem essa integração e não substitui esse trabalho por alterações visuais no progresso.
