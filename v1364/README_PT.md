# V13.6.4 TEST — jogos não instalados

Esta é uma atualização de teste sobre o **PKG V13.6.3 enviado pelo usuário**.
Não foi executada em um PS4. O CE-34878-0 ainda não tem causa confirmada;
os testes em computador não comprovam desbloqueio no firmware 13.52.

## Alterações

- A busca de metadados de jogos desinstalados passa a incluir
  `/data/TrophyUnlocker1352/metadata`, além das três pastas originais.
- Antes de tentar montar `trophy.img`, carrega `libkernel_sys.sprx`, resolve
  `statfs` e confere os quatro ponteiros da API TrophyData. Essa preparação
  segue o código público de referência; não é uma correção comprovada do CE.
- O buffer temporário do ioctl de decifragem reserva os `0x84` bytes indicados
  pelo comando. A V13.6.3 reservava `0x80` bytes para o array.
- Grava a última tentativa em
  `/data/TrophyUnlocker1352/v13_6_4-uninstalled.log`. O arquivo é reiniciado a
  cada tentativa de jogo desinstalado e não contém chaves nem metadados.
- Mantém os bytes da rotina comum de desbloqueio, os módulos, fontes, imagens,
  descoberta de jogos e interfaces da versão fornecida. Atualiza os rótulos
  de versão e o APP_VER para `01.11`; mantém o TITLE_ID `BREW13533`.

## Como testar

1. Instale `Trophy_Unlocker_13.52_V13_6_4_TEST.pkg` sobre a versão anterior.
   Guarde seu PKG V13.6.3 para referência. Não é necessário apagar o aplicativo.
2. Teste primeiro um troféu individual de um jogo instalado para verificar o
   comportamento que já funcionava. Depois teste um troféu individual de um
   jogo desinstalado que continua listado no mesmo usuário do PS4.
3. Se a tentativa falhar, copie o log indicado acima e anote o jogo/NPWR,
   troféu escolhido e a mensagem exibida. Se o processo fechar, a última linha
   indica a última etapa iniciada ou concluída, não necessariamente a causa.

## Quando faltam arquivos do jogo removido

Uma entrada no catálogo de troféus não garante que os metadados usados pela
rotina nativa ainda estejam disponíveis. Esta versão tenta os metadados
remanescentes e depois a montagem da imagem de troféus, como na V13.6.3.

Também aceita **os dois arquivos originais correspondentes ao mesmo jogo**,
copiados para uma subpasta, por exemplo:

- `/data/TrophyUnlocker1352/metadata/NPWR01234_00/npbind.dat`
- `/data/TrophyUnlocker1352/metadata/NPWR01234_00/nptitle.dat`

`NPWR01234_00` é apenas um exemplo. Use os arquivos reais do título/edição
correspondente, previamente preservados ou extraídos de sua própria cópia.
Não crie arquivos vazios, não renomeie arquivos de outro jogo e não substitua
arquivos da base de troféus. Se a busca encontrar os metadados correspondentes,
ela chama a mesma rotina nativa já usada pelo aplicativo, sem precisar montar
`trophy.img` para obtê-los. Isso não garante compatibilidade com todos os jogos.

## Reprodução e verificação

O fonte completo da V13.6.3 não estava no repositório disponível. Recompilar o
`v13/` teria retrocedido o aplicativo. Por isso esta alteração é um patch
binário documentado, exclusivo dos hashes abaixo, com hooks em C/assembly.
O patch recusa qualquer outro executável ou região de injeção ocupada.

Entrada PKG SHA256:
`ba29d8ab812e5a5bda33c65d6d6c84e79e66634b81c54ed2b71e15f18433a5d5`

Entrada eboot SHA256:
`ae91dde5d75a33ca2db9ca0458acbd6c418526ff08780367d6baf42c9f8adbc1`

Requer Python 3, GCC/binutils, `unicorn==2.1.4` e OpenOrbis PS4Toolchain
v0.5.4. Configure o ambiente de execução do PkgTool conforme seu sistema.

```sh
export OO_PS4_TOOLCHAIN=/caminho/PS4Toolchain
python3 v1364/build.py /caminho/Trophy_Unlocker_13.52_V13_6_3_Reference_Fix.pkg
```

O build executa oito grupos de testes dos hooks x86-64, incluindo duas bases
de carregamento para ASLR, argumentos, pilha, registradores, falhas de APIs,
buffer do ioctl e proteção da rotina comum. Verifica 28 itens de integridade
do PKG e extrai o resultado para comparar o executável e os arquivos originais.
Resultados ficam em `dist_v1364/`. **Isso não substitui o teste no PS4.**

## Referências utilizadas

- [PS4-vsh-utils: trophy.c](https://github.com/hzhreal/PS4-vsh-utils/blob/main/source/trophy.c)
  e [init.c](https://github.com/hzhreal/PS4-vsh-utils/blob/main/source/init.c):
  opções de montagem e resolução de `statfs`.
- [Apollo Save Tool: sd.c](https://github.com/bucanero/apollo-ps4/blob/master/source/sd.c):
  decifragem de sealedkey e montagem de imagens PFS.
- [Sony: CE-34878-0](https://www.playstation.com/pt-br/support/error-codes/ps4/ce-34878-0/):
  código genérico de erro no aplicativo; não identifica sozinho a rotina que falhou.
