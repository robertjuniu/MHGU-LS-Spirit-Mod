# MHGU LS Spirit Mod — v1.0.3

[Baixar addon](https://github.com/robertjuniu/MHGU-LS-Spirit-Mod/releases/download/v1.0.3/MHGU_LS_Spirit_Mod_v1.0.3.zip) · [Código e compilação](BUILDING.md)

MHGU 1.4.0 • Title ID 0100770008DD8000 • Eden Android / Nintendo Switch

Criador original: 东方朔3923
https://space.bilibili.com/pt/433307879

Port/adaptação standalone: robertjuniu. O original já inclui MHXX e MHGU;
este trabalho adapta sua implementação para addon ExeFS/RomFS independente,
com foco no Eden Android. Não exige Forge, root nem alteração do emulador.
Monster Hunter e os recursos do jogo pertencem à CAPCOM.

## Instalação

1. Feche o Eden completamente e faça backup do seu addon anterior.
2. Instale a pasta MHGU LS Spirit Mod deste ZIP como addon do MHGU.
   Na instalação manual, ela deve ficar dentro de load/0100770008DD8000/,
   contendo exefs/ e romfs/. Use a pasta de dados selecionada no seu Eden.
3. Remova/desative o antigo MHGU Iai Spirit Slash e outras versões deste
   mesmo mod. Não deixe dois addons de Long Sword ativos simultaneamente.
4. Ative o addon e inicie MHGU 1.4.0. Não use estes offsets em outra versão.

## Correção v1.0.3

ZR+Y agora é aceito durante o ataque X normal já inicializado, incluindo
quadros avançados e a segunda fase da ação. Foi removido o limite de seis
quadros imposto pelo port; o dispatcher original do autor foi executado
para confirmar esse comportamento em96 casos. Y sozinho continua disponível
para guardar a arma. Dano, estados aéreos e transições já pendentes continuam
protegidos. As correções de entrada rápida do Iai duplo permanecem.

## Continuação do Iai duplo

ZR+Y após o Iai Slash duplo agora inicia a estocada do Helm. Aceita X ainda
segurado e Y/ZR chegando separados. A pressão antecipada aguarda o fim
da segunda janela de impacto; B cancela normalmente. Y não escapa para
guardar a arma durante essa recuperação.

## Comandos

| Movimento | Comando |
|---|---|
| Special Sheathe | ZR+B |
| Iai Slash duplo | X desde o início da postura |
| Iai Spirit Slash | Soltar e apertar ZR novamente após preparar a postura |
| Foresight | ZR+A com arma sacada, fora da postura |
| Spirit Roundslash | Esquiva bem-sucedida de Foresight, acertar o retorno, novo ZR |
| Spirit Helm Breaker | ZR+Y; acertar a estocada; novo ZR perto do topo |

Na postura, A/ZR+A e Y isolado são bloqueados; ZR+Y continua disponível.
Na abertura da estocada do Helm, ZR+A pode trocar para Foresight. Depois
só B interrompe voluntariamente a preparação. O ZR do corte no ar é mantido.
O Helm gera sete impactos vinculados à parte atingida do monstro. A estocada
precisa acertar: não há aquisição automática de alvo nem acerto garantido.
Os coeficientes por impacto seguem o autor: sem cor8, branca10, amarela12,
vermelha15; o dano final depende da arma, hitzone e regras do jogo.

## Compatibilidade e validação

Testado pelo usuário no Eden Android em Galaxy S25. Não implica exclusividade
nem validação de todos os aparelhos. Outros emuladores e hardware Switch
não foram testados neste projeto. O executável alvo é o main A32 de MHGU1.4.0,
Build ID FB08F1D20FD1204F.

Verificados sem sobreposição estática: 60FPS, Reveal Map+Boss, Damage Numbers,
Key Quest Markers, FOV e pacotes HD instalados no aparelho de teste.
Mods que alterem os mesmos arquivos de animação/ataque da Long Sword ou
ocupem subsdk1 podem conflitar. Não substitui o APK, saves ou configurações.

A versão pública remove diagnósticos do protótipo e dados comprimidos antigos
sem uso nos ARC. Preserva clips, eventos, hitboxes e ritmo aprovado. As medidas
locais de instruções não equivalem a ganho de FPS nem a medição de bateria.
Pequenas imperfeições de vínculo da espada/bainha ainda podem aparecer.

## Créditos e direitos

Movimentos, animações, tabelas e efeitos originais: 东方朔3923.
Port standalone, integração e ajustes para Eden Android: robertjuniu.
Forge e sua variante consultados como referência; não são dependências.
A licença dos recursos autorais não acompanha o ZIP original recebido.
Este port não reivindica autoria nem concede licença sobre recursos de
terceiros. O nome e o link do criador devem acompanhar cópias deste addon.
