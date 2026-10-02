# Trophy Unlocker 13.52 — protótipo GoldHEN

Continuação atual da interface que abriu no PS4: **[V13.1 — correção do acesso GoldHEN e busca manual](v13/README_PT.md)**. Build: `bash ci/build_v13.sh`. [Auditoria e limites da validação](v13/AUDITORIA.md). A V13.1 corrige a checagem que impediu a tentativa de acesso na V13; o desbloqueio ainda não está integrado à UI. Os parágrafos abaixo descrevem o protótipo anterior.

Projeto mínimo para uso local/offline no PS4 13.52, focado em selecionar um jogo, carregar o contexto real de troféus criado pelo próprio jogo e chamar o serviço nativo `sceNpTrophyUnlockTrophy` sem editar diretamente `trophy_local.db`.

V5: o Manager agora usa `sceVideoOut` com framebuffer próprio para exibir uma interface visível no PS4; a V4 dependia apenas de notificações/log e por isso podia parecer travada em uma tela preta.

Não inclui código para PSN, sincronização, ocultação de alterações ou bypass de validações online.
