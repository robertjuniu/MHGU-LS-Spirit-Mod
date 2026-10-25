# Compilação reproduzível

O download em Releases já está pronto para instalar. Esta seção é para desenvolvimento.

Requisitos: Windows, Python3.10+ e Android NDK28.2.13676358 (LLVM). Não precisa de Forge nem de bibliotecas Python externas para compilar. Os testes históricos A32 usam Unicorn e fronteiras nativas simuladas; os relatórios publicados descrevem seu escopo.

1. Extraia legalmente seu MHGU1.4.0. Configure MHGU_GAME para a pasta com exefs/main e exefs/subsdk0.
2. Configure MHGU_NDK_BIN para toolchains/llvm/prebuilt/windows-x86_64/bin do NDK.
3. Baixe o addon v1.0.1 de Releases para inputs/MHGU_LS_Spirit_Mod_v1.0.1.zip, ou indique seu caminho em MHGU_LS_ADDON. Ele fornece os recursos RomFS já portados; o executável é recompilado do código-fonte.
4. Execute build.ps1. O pacote final estará em dist/. A verificação exige todos os arquivos idênticos à versão publicada.

src/ contém a integração atual. reference/r38/ e reference/work/ contêm as rotinas anteriores necessárias, reutilizadas sem depender do workspace privado. reference/generated/ conserva os trechos e tabelas ARM extraídos do mod de 东方朔3923; não são código de autoria do port. Nenhum main/subsdk0 do jogo ou log pessoal está incluído.

Ao desenvolver mudanças, adapte a checagem de identidade do pacote somente após revisar e testar o novo resultado; não reutilize relatórios antigos como validação de código novo.
