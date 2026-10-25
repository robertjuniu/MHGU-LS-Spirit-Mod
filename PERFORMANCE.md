# Desempenho e validação da v1.0.2

Esta versão consolida o comportamento aprovado do P17 e reduz trabalho desnecessário.

| Medida local | P17 | v1.0.1 |
|---|---:|---:|
| Instruções A32,100 chamadas de entrada em idle | 33.800 | 28.500 |
| Instruções A32,100 aplicações repetidas da pose em idle | 111.100 | 33.700 |
| Tamanho total dos10 ARC | 10.122.388 bytes | 6.843.583 bytes |
| Membros ativos comparados nos ARC | 243 | 243, mesmos bytes comprimidos |

Reduções nas rotinas medidas:15,68% e69,67%. Foram retirados3.278.805 bytes de
payloads comprimidos antigos já substituídos. Esse espaço não continha recursos
ativos. Cabeçalhos e flags dos membros, clips, eventos, animações e hitboxes foram
preservados; só os offsets externos dos payloads mudaram.

A medição usa100 chamadas idênticas após aquecimento, em Unicorn ARM32, com
fronteiras do motor simuladas. Não é benchmark de FPS, bateria ou temperatura no
Android. A reserva virtual do módulo permanece0x2C000 para não deslocar outros
addons. O tamanho descomprimido dos recursos ativos permanece igual; não afirmar
que retirar dados antigos reduz a memória de animações que o jogo precisa usar.

## Mudanças no runtime

- Os diagnósticos do protótipo e suas chamadas OutputDebugString não são emitidos
  pela versão pública; não há coleta contínua de botões ou log por impacto.
- A mesma verificação inicial do jogador é reutilizada na rotina de entrada.
- A regeneração já encerrada retorna antes das consultas de jogador/cena/relógio.
- A captura de uma pose idle idêntica evita repetir a validação matemática; mudanças
  de vínculo, recurso ou transform continuam exigindo validação. Três testes nativos
  cobrem pose igual, pose alterada e rejeição de NaN.
- Não foram acrescentadas threads, polling no Android, overlays ou alocações por
  quadro. Os recursos de efeito continuam tendo carga e liberação por cena.
- Foram mantidos os21 hooks e todas as proteções de dono/ator/ticket/versão.
  Não se ampliaram buscas de alvo ou hitboxes para mascarar erros de mira.

## Validação e limites

84.018 verificações C/Wasm, suites ARM de entrada, pose, vínculo, física, colisão,
trajetória, impactos, efeitos e Foresight passaram. O comparativo nativo da arma
mantém240 casos, erro máximo zero, mais quatro testes de restauração após troca e
três do caminho rápido de idle. Fixtures não comprovam renderização ou dano real.
Os relatórios em validation/ preservam o escopo de cada teste.

No P17 o usuário aprovou a aparência e confirmou a liberação de Roundslash pelo
acerto do retorno e dano do Helm considerado normal. O log registrou duas esquivas
Foresight e uma sequência completa de sete criações de impacto; não medir HP por
contagem de marcadores. A compilação otimizada ainda precisa de uma sessão longa
no aparelho para avaliar bateria, aquecimento e estabilidade em missões variadas.

## Comportamento mantido do original

Special Sheathe, Iai Spirit, Iai Slash, Foresight/retorno e Roundslash nativo,
estocada/salto/descida/pouso do Helm, sete impactos acompanhando a parte atingida,
sons e efeitos. Coeficientes do Helm8/10/12/15 seguem as tabelas originais.
As diferenças deliberadas do port incluem entradas antecipadas, bloqueio de A na
postura, recuperação para idle, tolerância maior das duas cápsulas em solo e o ritmo
de queda/corte calibrado pelo usuário. Não é anunciado como reprodução100% literal.

## Correção v1.0.1

ZR+Y agora continua o Iai Slash duplo para Helm após o fim da segunda janela
de impacto no quadro36. Uma pressão anterior fica no buffer do próprio movimento;
X ainda segurado não bloqueia a combinação e Y isolado não escapa para guardar
a arma. B cancela o pedido. Foram acrescentados75 casos, incluindo seis estilos,
recuperação, X segurado, chegada separada de Y/ZR e cancelamento do buffer.
Os dez ARC continuam idênticos aos da v1.0.0. A falha foi reproduzida em teste
antes da correção; a confirmação visual da atualização depende do usuário.

## Correção v1.0.2 — sequência rápida X → ZR+Y

O pedido do Helm agora é guardado mesmo quando o Iai duplo ainda aguarda
sua primeira inicialização de animação. Não exige que os botões continuem
segurados até o fim dos dois cortes. Após X normal, a janela inicial já
permitida aceita X ainda segurado e consome Y que chegue antes de ZR.
Ataques nativos avançados, estados inválidos e dano não são roubados.
Foram acrescentados54 casos reproduzindo os dois caminhos em seis estilos.
A falha do buffer anterior à inicialização foi reproduzida antes da correção.
Os diagnósticos continuam desligados e os dez ARC não mudaram. O grafo de
código estava indisponível nesta revisão; foram verificadas as fontes e
os testes diretamente. A sequência real precisa da confirmação do usuário.
