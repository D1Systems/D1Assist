# D1Assist

Protótipo C++20 de suporte remoto autorizado. A interface gráfica Qt 6 faz parte do aplicativo desde o primeiro marco. No Windows, o protótipo permite registrar dispositivos, solicitar uma sessão por ID, exigir consentimento explícito e senha temporária, exibir a tela remota e enviar eventos de mouse e teclado durante a sessão.

## Baixar para Windows

**[Baixar D1Assist para Windows (cliente, servidor e dependências)](https://github.com/D1Systems/D1Assist/releases/latest/download/D1Assist-Windows-x64.zip)**

O ZIP portátil inclui `D1Assist.exe`, `D1AssistServer.exe`, as DLLs do Qt e do runtime e os plugins necessários. Extraia o arquivo e execute os programas diretamente; não é necessário instalar Qt ou copiar DLLs manualmente. Cada nova tag `v*` cria uma Release com o pacote atualizado.

O link ficará disponível após a publicação da primeira tag de versão `v*` (por exemplo, `v0.1.0`).

> **Limite importante:** nesta versão, o servidor escuta somente em `127.0.0.1` e a comunicação não é criptografada. O pacote serve para testes locais no mesmo computador; ainda não permite suporte remoto entre computadores diferentes nem uso pela LAN/Internet.

## Compilar no Windows

Com Visual Studio, Qt 6 (módulo Widgets) e CMake disponíveis no ambiente:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Neste ambiente, o MinGW-w64 e Qt 6 estão disponíveis pelo MSYS2 e podem ser selecionados explicitamente:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER="C:\msys64\mingw64\bin\g++.exe" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

O build MinGW inclui estaticamente o runtime C++ do cliente. Após a compilação, `windeployqt` copia as DLLs e plugins Qt, e o CMake também inclui dependências transitivas do Qt, dos plugins de imagem e do MinGW ao lado do aplicativo. Com MinGW, os executáveis serão `build\server\D1AssistServer.exe` e `build\D1Assist.exe`; o Visual Studio adiciona o diretório da configuração, normalmente `Debug`.

Para gerar localmente o mesmo pacote portátil ZIP publicado nas Releases:

```powershell
cmake --build build --config Release
cpack --config build\CPackConfig.cmake -C Release -B build\packages
```

O arquivo será criado em `build\packages\D1Assist-Windows-x64.zip`.

O logotipo em `resources\D1AssistLogo.png` é distribuído ao lado do cliente para o ícone da janela Qt; `resources\D1Assist.ico` define o ícone incorporado aos executáveis Windows do cliente e do servidor.

## Testar uma sessão local no Windows

Em um terminal, inicie o servidor (ajuste o caminho se estiver usando Visual Studio):

```powershell
.\build\server\D1AssistServer.exe
```

Em outros dois terminais, abra duas janelas. Como ambas rodam no mesmo computador, use arquivos de identidade diferentes para simular dispositivos distintos:

```powershell
.\build\D1Assist.exe 127.0.0.1 5577 .\build\device-a.dat
.\build\D1Assist.exe 127.0.0.1 5577 .\build\device-b.dat
```

Em uma instalação normal, omita o terceiro argumento para usar a identidade persistente padrão. Nesta versão, porém, o servidor escuta somente em `127.0.0.1`; isso não permite conectar computadores distintos.

Cada janela mostra o ID público e uma senha temporária de seis dígitos. No computador que atuará como controlador, informe o ID e a senha da outra janela e clique em **Solicitar acesso**. Na janela chamada, aceite a solicitação; a senha é validada antes de iniciar a sessão. O visualizador abre após a autenticação e permite enviar mouse e teclado. A sessão permanece indicada no computador controlado e pode ser encerrada por qualquer participante.

O UUID e o ID público são salvos no diretório de dados do usuário; o servidor persiste o mapeamento em `server-data\devices.db`. Para escolher outra porta ou arquivos de persistência:

```powershell
.\build\D1AssistServer.exe 5578 .\server-data\devices-dev.db
.\build\D1Assist.exe 127.0.0.1 5578 .\device-a.dat
```

## Limites de segurança deste protótipo

O servidor escuta somente em `127.0.0.1`, e a comunicação TCP ainda não usa TLS. A senha, as imagens e os eventos de controle não são criptografados. Portanto, use somente o teste com duas instâncias no mesmo computador; não altere o servidor para escutar em outra interface nem use pela LAN/Internet. TLS e autenticação segura são requisitos antes de controlar outro computador pela rede. O consentimento visível e a senha são exigidos mesmo no teste local.

Como as duas instâncias de teste compartilham a mesma área de trabalho, a captura inclui ambas as janelas e eventos de mouse/teclado atuam no computador de teste. Encerre a sessão pelo botão visível antes de continuar usando o computador normalmente.
