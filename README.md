# Trophy Unlocker 13.52 — protótipo GoldHEN

Projeto mínimo para uso local/offline no PS4 13.52, focado em selecionar um jogo, carregar o contexto real de troféus criado pelo próprio jogo e chamar o serviço nativo `sceNpTrophyUnlockTrophy` sem editar diretamente `trophy_local.db`.

V5: o Manager agora usa `sceVideoOut` com framebuffer próprio para exibir uma interface visível no PS4; a V4 dependia apenas de notificações/log e por isso podia parecer travada em uma tela preta.

Não inclui código para PSN, sincronização, ocultação de alterações ou bypass de validações online.
