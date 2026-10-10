# D1Assist

Protótipo C++20 multiplataforma para suporte remoto autorizado. A interface gráfica Qt 6 faz parte do aplicativo desde o primeiro marco. Esta versão implementa o registro de dispositivos e a atribuição persistente de IDs; ainda não existe captura de tela, controle de mouse/teclado ou abertura de sessões remotas.

## Compilar no Windows

Com Visual Studio, Qt 6 (módulo Widgets) e CMake disponíveis no ambiente:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Neste ambiente, o MinGW-w64 e Qt 6 estão disponíveis pelo MSYS2 e podem ser selecionados explicitamente:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER="C:\msys64\mingw64\bin\g++.exe"
cmake --build build
```

O build MinGW inclui estaticamente o runtime C++ do cliente. Após a compilação, `windeployqt` copia as DLLs e plugins Qt, e o CMake também inclui dependências transitivas do Qt e do MinGW ao lado do aplicativo. Com MinGW, os executáveis serão `build\server\D1AssistServer.exe` e `build\D1Assist.exe`; o Visual Studio adiciona o diretório da configuração, normalmente `Debug`.

O logotipo em `resources\D1AssistLogo.png` é distribuído ao lado do cliente para o ícone da janela Qt; `resources\D1Assist.ico` define o ícone incorporado aos executáveis Windows do cliente e do servidor.

## Executar o primeiro marco

Em um terminal, inicie o servidor (ajuste o caminho se estiver usando Visual Studio):

```powershell
.\build\server\D1AssistServer.exe
```

Em outros dois terminais, abra duas janelas. Como ambas rodam no mesmo computador de teste, use arquivos de identidade diferentes para simular dois dispositivos:

```powershell
.\build\D1Assist.exe 127.0.0.1 5577 .\build\device-a.dat
.\build\D1Assist.exe 127.0.0.1 5577 .\build\device-b.dat
```

Em computadores diferentes, omita o terceiro argumento para cada instalação usar sua identidade padrão persistente.

Cada janela mostra o ID público, a senha temporária de seis dígitos e o estado de conexão. O UUID e o ID público são salvos no diretório de dados do usuário; o servidor persiste o mapeamento em `server-data\devices.db`. O botão para conectar a outro computador permanece desabilitado até existir autenticação e consentimento de sessão.

Para escolher outra porta ou arquivos de persistência:

```powershell
.\build\D1AssistServer.exe 5578 .\server-data\devices-dev.db
.\build\D1Assist.exe 127.0.0.1 5578 .\device-a.dat
```

## Limites de segurança deste protótipo

O servidor escuta somente em `127.0.0.1`, e a comunicação TCP ainda não usa TLS. Isso permite validar o protocolo localmente sem expor tráfego sem criptografia a outros computadores. Não use em redes externas nem para controle remoto real até implementar TLS, autenticação e autorização de sessão.
