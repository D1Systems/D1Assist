# D1Assist — AGENT.md

## 1. Visão do projeto

O **D1Assist** é um software de acesso e suporte remoto desenvolvido em C++ moderno.

O sistema deve permitir que um computador seja visualizado e controlado remotamente por outro computador através de:

- ID público;
- senha temporária;
- servidor central D1Assist;
- sessão remota autenticada.

A primeira versão será desenvolvida para Windows, mas toda a arquitetura deve ser preparada desde o início para suportar posteriormente:

- Windows;
- Linux;
- Android;
- iOS.

O projeto deve ser tratado como multiplataforma desde o primeiro commit.

---

# 2. Executável principal

O D1Assist NÃO deverá possuir executáveis separados para Host e Viewer.

Não criar:

```text
D1AssistHost.exe
D1AssistViewer.exe
```

Criar apenas:

```text
D1Assist.exe
```

O mesmo executável deve ser capaz de:

- receber conexão remota;
- iniciar conexão remota;
- mostrar seu próprio ID;
- mostrar sua senha temporária;
- conectar em outro ID;
- controlar outro computador;
- ser controlado por outro computador.

O papel pertence à sessão, e não ao executável.

---

# 3. Papéis de sessão

Criar conceito semelhante a:

```cpp
enum class SessionRole
{
    Controller,
    Controlled
};
```

`Controller`:

```text
máquina que está controlando outra
```

`Controlled`:

```text
máquina que está sendo controlada
```

O mesmo D1Assist poderá futuramente possuir múltiplas sessões.

---

# 4. Servidor

O projeto terá um servidor central separado:

```text
D1AssistServer
```

Inicialmente ele poderá reunir três responsabilidades:

```text
Broker
Authentication coordination
Relay
```

Posteriormente essas responsabilidades poderão ser separadas.

O servidor deverá:

- manter dispositivos online;
- mapear ID público para conexão ativa;
- encaminhar solicitação de conexão;
- criar sessões;
- inicialmente encaminhar tráfego entre clientes;
- futuramente ajudar no estabelecimento de conexão P2P.

---

# 5. Arquitetura inicial

```text
                 D1AssistServer
                ┌───────────────┐
                │ Broker        │
                │ Sessions      │
                │ Relay         │
                └───────┬───────┘
                        │
               ┌────────┴────────┐
               │                 │
               ▼                 ▼
         D1Assist.exe       D1Assist.exe
            PC A                PC B
```

Exemplo:

```text
PC A
ID: 834 221 557
Senha: 583921

PC B
digita:
834221557
583921
```

O servidor localiza o PC A e inicia o fluxo de autenticação.

---

# 6. Fluxo de inicialização

Ao iniciar:

```text
D1Assist.exe
      ↓
carrega identidade local
      ↓
conecta ao D1AssistServer
      ↓
REGISTER_DEVICE
      ↓
servidor registra dispositivo
      ↓
servidor devolve ID público
      ↓
D1Assist gera senha temporária
      ↓
interface mostra ID + senha
```

Exemplo:

```text
Seu ID:
834 221 557

Senha:
583 921
```

---

# 7. Identidade do dispositivo

Cada instalação deverá possuir uma identidade interna persistente.

Exemplo:

```text
device_uuid:
8f3a9e52-50e2-4df5-b962-3897fc713249
```

Além disso, deverá possuir um ID público amigável:

```text
public_id:
834221557
```

O usuário verá apenas:

```text
834 221 557
```

Nunca gerar o ID diretamente a partir de:

- MAC address;
- serial de HD;
- IP;
- nome da máquina.

---

# 8. DeviceIdentity

Criar classe:

```cpp
class DeviceIdentity
{
public:
    bool loadOrCreate();

    const std::string& deviceUuid() const;
    std::uint64_t publicId() const;

    void setPublicId(std::uint64_t id);

private:
    std::string deviceUuid_;
    std::uint64_t publicId_ = 0;
};
```

Responsabilidade:

- criar UUID na primeira execução;
- salvar UUID localmente;
- carregar UUID nas próximas execuções;
- armazenar ID público recebido do servidor.

---

# 9. Senha temporária

A primeira versão deverá utilizar senha temporária.

Inicialmente:

```text
6 dígitos
```

Exemplo:

```text
583921
```

A senha deve ser regenerada:

```text
a cada inicialização
```

ou quando o usuário solicitar nova senha.

Criar:

```cpp
class PasswordManager
{
public:
    void regenerate();

    std::string password() const;

    bool validate(const std::string& password) const;

private:
    std::string password_;
};
```

A senha nunca deve ser registrada em logs.

---

# 10. Fluxo de conexão por ID

PC B deseja acessar PC A.

Fluxo:

```text
PC B
 |
 | CONNECT_REQUEST
 | target_id = 834221557
 |
 ▼
D1AssistServer
 |
 | procura target_id
 |
 ▼
PC A
 |
 | INCOMING_CONNECTION
 |
 ▼
fluxo de autenticação
```

Depois:

```text
PC B envia autenticação
        ↓
PC A valida
        ↓
AUTH_ACCEPTED
        ↓
SESSION_START
```

Se inválida:

```text
AUTH_DENIED
```

---

# 11. Regra de autorização

O computador controlado deverá participar da autorização da sessão.

O servidor não deverá simplesmente conceder acesso remoto arbitrariamente.

A sessão somente poderá iniciar se a autenticação for aceita.

O usuário controlado deve:

- saber que existe sessão ativa;
- possuir forma visível de encerrar a sessão;
- receber indicação de conexão ativa.

---

# 12. Protocolo inicial

Criar enum:

```cpp
enum class MessageType : std::uint16_t
{
    Hello = 1,
    HelloAck = 2,

    RegisterDevice = 10,
    RegisterDeviceResponse = 11,

    ConnectRequest = 20,
    IncomingConnection = 21,
    ConnectRejected = 22,

    AuthRequest = 30,
    AuthResponse = 31,
    AuthAccepted = 32,
    AuthDenied = 33,

    SessionStart = 40,
    SessionEnd = 41,

    Frame = 100,
    MouseMove = 101,
    MouseButton = 102,
    MouseWheel = 103,

    KeyDown = 110,
    KeyUp = 111,

    Clipboard = 120,

    FileTransferStart = 130,
    FileTransferData = 131,
    FileTransferEnd = 132
};
```

---

# 13. PacketHeader

Criar cabeçalho binário padrão.

Exemplo conceitual:

```cpp
struct PacketHeader
{
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t type;
    std::uint32_t payloadSize;
};
```

O protocolo deverá possuir:

```text
magic
version
message type
payload size
payload
```

Nunca confiar em valores recebidos da rede.

Validar sempre:

- tamanho;
- versão;
- tipo;
- limites;
- estado da sessão.

---

# 14. Serialização

Nunca assumir que uma struct C++ pode ser enviada diretamente pela rede.

Evitar protocolo baseado em:

```cpp
send(socket, &myStruct, sizeof(myStruct), ...);
```

Criar:

```text
PacketWriter
PacketReader
```

Responsáveis por serialização explícita.

Motivos:

- padding;
- endianess;
- compiladores diferentes;
- sistemas diferentes;
- arquiteturas diferentes.

---

# 15. Estrutura do projeto

```text
D1Assist/
│
├── CMakeLists.txt
├── README.md
├── AGENT.md
│
├── src/
│   │
│   ├── app/
│   │   └── main.cpp
│   │
│   ├── core/
│   │   ├── common/
│   │   ├── identity/
│   │   ├── auth/
│   │   ├── network/
│   │   ├── protocol/
│   │   ├── session/
│   │   ├── capture/
│   │   └── input/
│   │
│   └── platform/
│       ├── windows/
│       └── linux/
│
├── server/
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp
│       ├── broker/
│       ├── sessions/
│       └── relay/
│
├── include/
│   └── d1assist/
│
├── tests/
└── docs/
```

---

# 16. Linguagem

Utilizar:

```text
C++20
```

Preferir C++ moderno.

Utilizar quando fizer sentido:

```cpp
std::string
std::vector
std::array
std::unique_ptr
std::shared_ptr
std::optional
std::filesystem
std::thread
std::mutex
std::condition_variable
std::chrono
std::atomic
```

Evitar:

```cpp
malloc()
calloc()
realloc()
free()
```

Evitar `new` e `delete` diretamente sempre que RAII e smart pointers forem suficientes.

---

# 17. Ensino de C++

O desenvolvedor principal possui experiência forte em Delphi/Object Pascal e está aprendendo C++ através deste projeto.

Sempre que introduzir conceitos importantes, explicar:

1. o que significa;
2. por que está sendo usado;
3. como funciona;
4. o que acontece na memória;
5. qual seria o paralelo aproximado em Delphi.

Especialmente explicar:

```text
pointer
reference
const
constexpr
auto
unique_ptr
shared_ptr
move
RAII
template
lambda
vector
optional
thread
mutex
atomic
virtual
override
```

Exemplo:

```cpp
std::unique_ptr<BrokerClient> broker_;
```

Explicar que:

- existe um único dono do objeto;
- ele será destruído automaticamente;
- não é necessário chamar `delete`;
- isso ajuda a evitar memory leak.

---

# 18. Regra multiplataforma

O diretório:

```text
src/core/
```

não deverá depender diretamente de:

```text
Windows.h
WinSock
DXGI
X11
Wayland
PipeWire
UIKit
Android API
```

Código específico deverá ficar em:

```text
src/platform/
```

Exemplo:

```text
src/core/capture/screen_capture.h

src/platform/windows/screen_capture_windows.cpp

src/platform/linux/screen_capture_linux.cpp
```

---

# 19. CMake

Utilizar CMake desde o início.

Compilação:

```bash
cmake -S . -B build
cmake --build build
```

No Windows:

```bash
cmake -S . -B build
cmake --build build --config Debug
```

Compiladores:

Windows:

```text
MSVC
```

Linux:

```text
GCC ou Clang
```

---

# 20. Interface gráfica obrigatória

O D1Assist sempre deverá possuir interface gráfica. A GUI faz parte do produto desde o primeiro marco e deve acompanhar toda funcionalidade nova; não criar etapas funcionais que dependam de interação pelo console.

Utilizar Qt 6 e criar uma interface inicial simples semelhante a:

```text
┌────────────────────────────────────┐
│             D1Assist               │
│                                    │
│  Este dispositivo                  │
│                                    │
│  Seu ID                            │
│  834 221 557                       │
│                                    │
│  Senha                             │
│  583 921                           │
│                                    │
│  ───────────────────────────────   │
│                                    │
│  Acessar outro computador          │
│                                    │
│  ID remoto                         │
│  [____________________]            │
│                                    │
│  Senha                             │
│  [____________]                    │
│                                    │
│       [ Conectar ]                 │
└────────────────────────────────────┘
```

Durante o desenvolvimento, controles de funcionalidades ainda não implementadas devem aparecer desabilitados ou explicar seu estado; não substituir a GUI por comandos de console.

---

# 21. BrokerClient

Criar classe:

```cpp
class BrokerClient
{
public:
    bool connect(
        const std::string& host,
        std::uint16_t port
    );

    bool registerDevice(
        const DeviceIdentity& identity
    );

    bool requestConnection(
        std::uint64_t remoteId
    );

    void disconnect();

private:
    // transporte
};
```

Responsabilidades:

- conectar ao servidor D1Assist;
- manter conexão ativa;
- registrar dispositivo;
- receber public ID;
- pedir conexão;
- receber solicitações;
- iniciar sessão.

---

# 22. RemoteSession

Criar:

```cpp
class RemoteSession
{
public:
    RemoteSession(SessionRole role);

    bool start();
    void stop();

    SessionRole role() const;

private:
    SessionRole role_;
    std::atomic<bool> running_{false};
};
```

Posteriormente coordenará:

```text
transport
protocol
capture
input
encoder
decoder
```

---

# 23. Servidor — estrutura inicial

O servidor deverá manter algo conceitualmente semelhante a:

```text
public_id -> conexão ativa
```

Exemplo:

```text
834221557 -> socket/conexão do PC A
921450311 -> socket/conexão do PC B
```

Nunca confiar exclusivamente em ID público como identidade interna.

Manter também:

```text
device_uuid
connection_id
session_id
```

---

# 24. Geração de public ID

O servidor deverá ser responsável por gerar ou atribuir o `public_id`.

Exemplo:

```text
834221557
```

Requisitos:

- evitar colisão;
- não ser derivado diretamente de hardware;
- permitir associação persistente ao `device_uuid`;
- ser amigável para digitação.

---

# 25. Sessão

Quando houver conexão autorizada, criar:

```text
session_id
```

Exemplo conceitual:

```text
Controller:
921450311

Controlled:
834221557

Session:
c511f123...
```

A partir daí, os pacotes deverão estar associados à sessão correta.

---

# 26. Relay inicial

Para simplificar o MVP, inicialmente o tráfego remoto poderá passar pelo servidor:

```text
PC A
  ↓
D1AssistServer
  ↓
PC B
```

Isso inclui futuramente:

```text
frames
mouse
teclado
clipboard
```

Depois migrar para:

```text
PC A ←→ PC B
```

quando P2P estiver disponível.

---

# 27. NAT traversal

Não implementar na primeira versão.

Roadmap futuro:

```text
STUN
TURN
ICE
UDP
QUIC
WebRTC
```

Primeiro fazer o produto funcionar através do relay.

---

# 28. Rede

Separar sempre:

```text
Capture
   ↓
Encoder
   ↓
Protocol
   ↓
Transport
   ↓
Network
```

Nunca acoplar captura diretamente ao socket.

---

# 29. Transport

Criar abstração semelhante a:

```cpp
class ITransport
{
public:
    virtual ~ITransport() = default;

    virtual bool connect(
        const std::string& host,
        std::uint16_t port
    ) = 0;

    virtual bool send(
        const std::uint8_t* data,
        std::size_t size
    ) = 0;

    virtual void disconnect() = 0;
};
```

O core não deverá depender diretamente de WinSock.

---

# 30. Captura

Primeira plataforma:

```text
Windows
```

Pode existir implementação simples para protótipo.

A implementação definitiva deverá caminhar para:

```text
DXGI Desktop Duplication API
```

Preparar suporte futuro a:

```text
múltiplos monitores
cursor
mudança de resolução
regiões alteradas
GPU
```

---

# 31. Linux

O projeto deverá prever desde o início:

```text
X11
Wayland
PipeWire
xdg-desktop-portal
```

Não assumir que Linux = X11.

---

# 32. Mobile

Futuramente:

Android:

```text
Kotlin
```

iOS:

```text
Swift
```

Inicialmente mobile deverá atuar como Viewer.

---

# 33. Input remoto

Windows:

```text
SendInput
```

Linux futuramente:

```text
XTest
uinput
Wayland/compositor APIs
```

Nunca implementar captura clandestina de teclado.

Os eventos devem existir somente durante uma sessão remota autorizada.

---

# 34. Segurança

O projeto deve ser desenvolvido como software legítimo de suporte remoto.

Nunca implementar:

- instalação escondida;
- persistência oculta;
- keylogger;
- bypass de consentimento;
- acesso sem autenticação;
- ocultação de sessão remota.

O usuário controlado deverá sempre possuir indicação de sessão ativa.

---

# 35. Criptografia

A primeira prova de conceito poderá utilizar TCP simples somente em ambiente local de desenvolvimento.

Antes de uso real externo, implementar criptografia.

Planejar:

```text
TLS
autenticação segura
proteção contra replay
nonce
session token
criptografia ponta a ponta
```

Nunca criar algoritmo criptográfico próprio.

---

# 36. Logs

Criar:

```cpp
enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error
};
```

Registrar:

```text
server connected
device registered
public id assigned
incoming connection
authentication accepted
session started
session ended
network error
```

Nunca registrar:

```text
senha
token
conteúdo do clipboard
frames
chaves criptográficas
```

---

# 37. Threads

Separar responsabilidades.

Inicialmente:

```text
Main Thread
Network Thread
Session Thread
```

Depois:

```text
Capture Thread
Encoder Thread
Decoder Thread
Input Thread
```

Utilizar:

```cpp
std::thread
std::mutex
std::lock_guard
std::condition_variable
std::atomic
```

Não criar threads sem necessidade.

---

# 38. Convenções

Namespace:

```cpp
namespace d1assist
{
}
```

Subnamespaces:

```cpp
namespace d1assist::network
{
}

namespace d1assist::protocol
{
}

namespace d1assist::session
{
}
```

Classes:

```text
DeviceIdentity
PasswordManager
BrokerClient
RemoteSession
TcpTransport
PacketReader
PacketWriter
ScreenCapture
```

Membros privados:

```cpp
running_
socket_
identity_
broker_
session_
```

Constantes:

```cpp
constexpr std::uint16_t DEFAULT_SERVER_PORT = 5577;
```

---

# 39. Primeira meta real

A primeira implementação NÃO será captura de tela.

Implementar primeiro:

```text
D1AssistServer
+
D1Assist.exe
```

Fluxo mínimo:

```text
D1Assist.exe
      ↓
conecta ao servidor
      ↓
REGISTER_DEVICE
      ↓
servidor atribui ID
      ↓
cliente mostra ID
```

Abrir segunda instância:

```text
D1Assist.exe
      ↓
também registra
      ↓
recebe outro ID
```

Depois:

```text
Instância B
      ↓
CONNECT_REQUEST
target_id = ID da Instância A
      ↓
Servidor encontra A
      ↓
INCOMING_CONNECTION
      ↓
autenticação
      ↓
SESSION_START
```

Essa será a primeira versão funcional da arquitetura.

---

# 40. Fase 1 — Registro

Implementar:

```text
TCP Server
TCP Client
DeviceIdentity
RegisterDevice
RegisterDeviceResponse
```

Resultado esperado:

```text
[D1Assist]

Connected to server.

Device UUID:
8f3a9e52-...

Your ID:
834 221 557

Password:
583921
```

---

# 41. Fase 2 — Conexão pelo ID

Implementar:

```text
ConnectRequest
IncomingConnection
```

Teste:

```text
PC B informa:
834221557
```

Servidor localiza o PC A.

---

# 42. Fase 3 — Autenticação

Implementar:

```text
AuthRequest
AuthResponse
AuthAccepted
AuthDenied
```

Somente criar sessão após senha correta.

---

# 43. Fase 4 — Sessão

Criar:

```text
SessionStart
SessionEnd
session_id
RemoteSession
```

---

# 44. Fase 5 — Captura local

Implementar captura de tela Windows.

Validar primeiro localmente.

---

# 45. Fase 6 — Frame remoto

Enviar frame através da sessão.

Inicialmente:

```text
JPEG ou WebP
```

Não enviar bitmap bruto indefinidamente.

---

# 46. Fase 7 — Viewer

Criar janela para renderizar tela remota.

---

# 47. Fase 8 — Mouse

Implementar:

```text
MouseMove
MouseButton
MouseWheel
```

---

# 48. Fase 9 — Teclado

Implementar:

```text
KeyDown
KeyUp
```

---

# 49. Fase 10 — Clipboard

Implementar clipboard controlado por sessão.

---

# 50. Fase 11 — Transferência de arquivos

Adicionar posteriormente.

---

# 51. Fase 12 — Criptografia completa

Antes de disponibilizar externamente.

---

# 52. Fase 13 — P2P

Depois que o relay estiver estável.

---

# 53. Fase 14 — Linux

Primeiro:

```text
compilar core
```

Depois:

```text
Linux Viewer
```

Depois:

```text
Linux Host
```

Separar:

```text
X11
Wayland
```

---

# 54. Fase 15 — Mobile

Primeiro:

```text
Android Viewer
```

Depois:

```text
iOS Viewer
```

---

# 55. Princípios

Sempre priorizar:

```text
funcionar
↓
ser correto
↓
ser seguro
↓
ser legível
↓
ser multiplataforma
↓
ser eficiente
↓
otimizar
```

Não otimizar prematuramente.

---

# 56. Regra de evolução

Não reescrever o projeto inteiro a cada nova funcionalidade.

Antes de uma mudança arquitetural:

1. identificar o problema;
2. explicar a limitação atual;
3. propor solução;
4. preservar compatibilidade quando possível.

---

# 57. Primeira tarefa para o agente

Começar criando:

```text
D1AssistServer
D1Assist.exe
```

Objetivo exclusivo da primeira tarefa:

```text
1. servidor iniciar;
2. cliente conectar;
3. cliente enviar REGISTER_DEVICE;
4. servidor gerar/recuperar public_id;
5. servidor responder;
6. cliente exibir public_id;
7. cliente gerar senha temporária;
8. duas instâncias com GUI funcionarem simultaneamente.
```

Nesta primeira tarefa, a GUI já é obrigatória. Ainda NÃO implementar:

```text
captura de tela
mouse
teclado
P2P
NAT traversal
codec avançado
transferência de arquivos
```

Depois implementar:

```text
CONNECT_REQUEST
+
senha
+
SESSION_START
```

antes de iniciar o streaming remoto.

---

# 58. Estado desejado do primeiro marco

Abrir duas instâncias:

```text
D1Assist.exe A

Seu ID:
834 221 557

Senha:
583921
```

e:

```text
D1Assist.exe B

Seu ID:
921 450 311

Senha:
174820
```

Na instância B:

```text
Conectar ao ID:
834221557

Senha:
583921
```

Resultado:

```text
Connection authenticated.
Session started.
```

Esse será o primeiro marco oficial do D1Assist.