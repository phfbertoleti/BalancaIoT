/*
 * Projeto: balança IoT - pesagem
 * Autor: Pedro Bertoleti
 * Data: Novembro/2017
 */

 /*
  * bibliotecas utilizadas:
  * MQTT (PubSubClient): https://github.com/knolleary/pubsubclient
  */

//includes
#include <EEPROM.h>
#include <ESP8266WiFi.h> // Importa a Biblioteca ESP8266WiFi
#include <PubSubClient.h> // Importa a Biblioteca PubSubClient

//- defines
//1) gerais
#define SIM                               1
#define NAO                               0
#define NAO_APLICAVEL                     2

//descomente a linha abaixo para testar somente a leitura do ADC
//#define FAZ_SOMENTE_LEITURA_ADC

//2)ADC e pesagem:
#define CANAL_ADC_PLATAFORMA              0  //utiliza o canal 0 do adc para leitura de plataforma
#define ZERA_PLATAFORMA_VALOR_CONTADOR    16
#define PESO_MINIMO_CALIBRACAO            0.0     //g
#define PESO_MAXIMO_CALIBRACAO            5000.0  //g
#define NUMERO_LEITURAS_COUNTS_CALIBRACAO 100
#define NUMERO_AMOSTRAS_ADC_POR_SEGUNDO   7
#define NUMERO_LEITURAS_ADC_FILTRAGEM     50
#define JANELA_COUNTS_CALIBRACAO          10

//3) Inputs e outputs
#define D0                                16
#define D1                                5
#define D2                                4
#define D3                                0
#define D4                                2
#define D5                                14
#define D6                                12
#define D7                                13
#define D8                                15
#define D9                                3
#define D10                               1
#define BREATHING_LIGHT                   D0
#define BOTAO_CALIBRAR                    D1
#define LED_INDICATIVO_BALANCA_CALIBRADA  D2
#define LED_ACESO                         1
#define LED_APAGADO                       0

//4) UARTS
#define BAUDRATE_SERIAL_MONITOR           115200

//5) Temporizações
#define TEMPO_BREATHINGLIGHT                                  500
#define TEMPO_ENVIO_MQTT                                      150
#define TEMPO_ENVIO_FUNCIONAMENTO_THINGSPEAK_FUNCIONAMENTO    30000  
#define TEMPO_ENVIO_FUNCIONAMENTO_THINGSPEAK_PESAGENS         16000  

//6) EEPROM Emulada
#define ENDERECO_EEPROM_EMULADA           0

//7) MQTT / IoT
//defines de id mqtt e tópicos para publicação e subscribe
#define TOPICO_SUBSCRIBE "MQTTBalancaRecebe"     //tópico MQTT de escuta
#define TOPICO_PUBLISH   "MQTTBalancaEnvia"    //tópico MQTT de envio de informações para Broker
                                                //IMPORTANTE: ao utilizar brokers de uso geral / livres, recomenda-se fortemente 
                                                //            alterar os nomes desses tópicos. Caso contrário, há grandes
                                                //            chances de você controlar e monitorar o NodeMCU
                                                //            de outra pessoa.
#define ID_MQTT  "BalancaIoTPHFB"     //id mqtt (para identificação de sessão)
                                      //IMPORTANTE: este deve ser único no broker (ou seja, 
                                      //            se um client MQTT tentar entrar com o mesmo 
                                      //            id de outro já conectado ao broker, o broker 
                                      //            irá fechar a conexão de um deles).

//typedefs / structs
typedef struct {
  int FlagBalancaCalibrada;  //Se igual a 0: balança não calibrada
                              //Se igual a 1: balança calibrada
  float CoefAngular;
  float CoefLinear; 
  int CountsPesoMinimo;
  int CountsPesoMaximo;                               
} TCalibracao;

typedef struct {
  int ADCCounts;             //contém última leitura de counts de ADC
  float PesoKG;              //contém último valor de peso (em g)

  float PesoAcumuladoKG;     //contém o peso acumulado total (em g) das pesagens confirmadas
  float PrecoAcumulado;      //contém o preço acumulado total (em R$) das pesagens confirmadas
   
  float ValorPorKG;          //valor (em R$) por kg
  float ValorTotalPesagem;   //valor (em R$) da pesagem
  
  
  char FlagPlataformaZerada;  //Se igual a 0: plataforma ainda não está zerada
                              //Se igual a 1: plataforma foi zerada
  
  char FlagPlataformaEstavel; //Se igual a 0: plataforma instável
                              //Se igual a 1: plataforma estável

  TCalibracao CalibracaoBalanca;
}TPesagem;

//variaveis globais
TPesagem  InfoPesagem;
float PesoAnterior;
int TempoEntreConversoesADC;
char EstadoBreathingLight;
unsigned long UltimaTemporizacao_BreatlingLight;
unsigned long UltimaTemporizacao_LeituraADC;
unsigned long UltimaTemporizacao_EnvioMQTT;
unsigned long UltimaTemporizacao_EnvioThingSpeak_Funcionamento;
unsigned long UltimaTemporizacao_EnvioThingSpeak_Pesagens;
unsigned long UltimaTemporizacao_FuncionamentoBalanca;
unsigned long TempoDeFuncionamento_Demonstracao;

//variaveis globais - WIFI
const char* SSID = "ColoqueSeuSSID";      // SSID / nome da rede WI-FI que deseja se conectar
const char* PASSWORD = "ColoqueSeuPassword";       // Senha da rede WI-FI que deseja se conectar
 
//variaveis globais - MQTT
const char* BROKER_MQTT = "iot.eclipse.org"; //URL do broker MQTT que se deseja utilizar
int BROKER_PORT = 1883;                      // Porta do Broker MQTT
WiFiClient espClient;                        // Cria o objeto espClient
PubSubClient MQTT(espClient);                // Instancia o Cliente MQTT passando o objeto espClient
char AcionamentoBotaoVirtualPesagem;         //Informa se botão virtual de confirmação de pesagem foi acionado

//variáveis globais - ThingSpeak
char EnderecoAPIThingSpeak[] = "api.thingspeak.com";                     //endereço para fazer requisição HTTP ao ThingSpeak
String ChaveEscritaThingSpeak_PesagensConfirmadas = "kkkkkkkkkkkkkkkk";  //substitua "kkkkkkkkkkkkkkkk" pela sua chave de escrita thingspeak (canal de pesagens confirmadas)
String ChaveEscritaThingSpeak_Funcionamento = "kkkkkkkkkkkkkkkk";        //substitua "kkkkkkkkkkkkkkkk" pela sua chave de escrita thingspeak (canal de histórico de funcionamentos)

//prototypes
void ToogleBreathingLight(void);
int FazLeituraADC(void);
void FazPesagem(void);
float CalculaPeso(int LeituraADC);
char VerificaBotaoCalibrar(void);
void VerificaBotaoVirtualConfirmacaoPesagem(void);
char FazCalibracao(void);
int CalibraPesoMinimoOuMaximo(void);
void LeADCEFazPesagem(void);
void CalculaPrecoPesagem(void);
void SalvaCalibracao(void);
void LeCalibracao(void) ;
unsigned long TempoDecorrido(unsigned long Referencia);
void AguardaApertarBotaoCal(void);
void AguardaSoltarBotaoCal(void);
void EnviaSerialInfosCalibracao(void);
void InitWiFi(void);
void InitMQTT(void);
void ReconectaMQTT(void); 
void ReconectaWiFi(void); 
void MQTTCallback(char* topic, byte* payload, unsigned int length);
void VerificaConexoesWiFIEMQTT(void);
void VerificaSeDeveEnviarDadosMQTT(void);
void ConfirmaPesagem(void);
void EnviaInformacoesPesagemConfirmadaThingspeak(float DadosField1, float DadosField2);
void VerificaEnvioKeepAliveFuncionamentoThingspeak(void);
void EscreveMensagemDebugPesagem(void);
void IniciaContagemTempoManutencaoPreventiva(void);
char VerificaSeGeraAlertaManutencaoPreventiva(void);

/* 
 *  Implementações
 */
//Função: faz o breathing light piscar
//Parametros: nenhum
//Retorno: nenhum
void ToogleBreathingLight(void)
{  
    if (EstadoBreathingLight == LED_ACESO)
    {
      digitalWrite(BREATHING_LIGHT, LOW);
      EstadoBreathingLight = LED_APAGADO;  
    }
    else
    {
      digitalWrite(BREATHING_LIGHT, HIGH);
      EstadoBreathingLight = LED_ACESO;  
    }
    
    UltimaTemporizacao_BreatlingLight = millis();
}

//Função: informa o tempo decorrido (em milisegundos) a partir da referencia
//Parametros: referência
//Retorno: tempo decorrido (ms)
unsigned long TempoDecorrido(unsigned long Referencia)
{
  return (millis() - Referencia);
}

//Função: faz leitura do ADC (com filtragem simples, por média)
//Parametros: nenhum
//Retorno: leitura do ADC(0 - 1024)
int FazLeituraADC(void)
{
  unsigned long SomaLeiturasADC;
  int LeituraADCObtida;
  char i;

  SomaLeiturasADC = 0;

  //faz 'NUMERO_LEITURAS_ADC_FILTRAGEM' leituras de ADC
  for (i=0; i<NUMERO_LEITURAS_ADC_FILTRAGEM; i++)
  {
    SomaLeiturasADC = SomaLeiturasADC + analogRead(CANAL_ADC_PLATAFORMA);
    ESP.wdtFeed();
  }
  
  LeituraADCObtida = SomaLeiturasADC/NUMERO_LEITURAS_ADC_FILTRAGEM;
  UltimaTemporizacao_LeituraADC = millis();
  return LeituraADCObtida;
}

//Função: calcula peso
//Parametros: leitura do ADC
//Retorno: peso em kg
float CalculaPeso(int LeituraADC) 
{
  float PesoCalculado = 0.0;
  PesoCalculado = InfoPesagem.CalibracaoBalanca.CoefLinear + ((float)LeituraADC * InfoPesagem.CalibracaoBalanca.CoefAngular);
  return PesoCalculado;
}

//Função: faz pesagem (em kg)
//Parametros: nenhum
//Retorno: nenhum
void FazPesagem(void)
{
  InfoPesagem.PesoKG = CalculaPeso(InfoPesagem.ADCCounts);

  if (InfoPesagem.PesoKG == 0)
    InfoPesagem.FlagPlataformaZerada = SIM;
  else
    InfoPesagem.FlagPlataformaZerada = NAO;  

  if (PesoAnterior != InfoPesagem.PesoKG)
  {
    InfoPesagem.FlagPlataformaEstavel = NAO;
    PesoAnterior = InfoPesagem.PesoKG;
  }
  else
    InfoPesagem.FlagPlataformaEstavel = SIM;
}

//Função: le o ADC e faz a pesagem
//Parametros: nenhum
//Retorno: nenhum
void LeADCEFazPesagem(void)
{
  InfoPesagem.ADCCounts = FazLeituraADC();

  //se há calibração feita, faz pesagem. Se a pesagem for b em sucedida, envia dados de pesagem para OpenWRT
  if (InfoPesagem.CalibracaoBalanca.FlagBalancaCalibrada == SIM) 
  {
    digitalWrite(LED_INDICATIVO_BALANCA_CALIBRADA, HIGH);
    FazPesagem();  
  }
  else
    digitalWrite(LED_INDICATIVO_BALANCA_CALIBRADA, LOW);  
}

//Função: calcula o valor total da pesagem
//Parametros: nenhum
//Retorno: nenhum
void CalculaPrecoPesagem(void)
{
    InfoPesagem.ValorTotalPesagem = InfoPesagem.ValorPorKG*(InfoPesagem.PesoKG/1000); 
}

//Função: verifica se deve iniciar a calibração
//Parametros: nenhum
//Retorno: nenhum
void VerificaBotaoVirtualConfirmacaoPesagem(void)
{
   if (AcionamentoBotaoVirtualPesagem == NAO)
     return;
   
   ConfirmaPesagem();
   AcionamentoBotaoVirtualPesagem = NAO;
}

//Função: verifica se deve iniciar a calibração
//Parametros: nenhum
//Retorno: SIM - botao apertado
//         NAO - botao nao apertado
char VerificaBotaoCalibrar(void)
{
  if (!digitalRead(BOTAO_CALIBRAR))
    delay(20); //20ms de tempo para debounce
  else
   return NAO;  
  
  if (!digitalRead(BOTAO_CALIBRAR))
  {
    //acionamento do botão de calibração foi confirmado.     
    return SIM;
  }
  else
    return NAO;    
}

//Função: aguarda usuario apertar botao de calibração
//Parametros: nenhum
//Retorno: nenhum
void AguardaApertarBotaoCal(void)
{
  while(VerificaBotaoCalibrar() == NAO)
  {
    //restarta WDT
    ESP.wdtFeed();  
  }
}

//Função: aguarda usuario soltar botao de calibração
//Parametros: nenhum
//Retorno: nenhum
void AguardaSoltarBotaoCal(void)
{
  while(VerificaBotaoCalibrar() == SIM)
  {
    //restarta WDT
    ESP.wdtFeed();  
  }
}

//Função: faz processo de calibração
//Parametros: nenhum
//Retorno: SIM - calibração bem sucedida
//         NAO - falha na calibração
//
// IMPORTANTE: por razões do sensor de força resitivo,
//             o peso maximo é fxo em 5kg
char FazCalibracao(void) 
{
  int CountsPesoMinimo;
  int CountsPesoMaximo;

  Serial.println("------------------------------");
  Serial.println("    Processo de calibracao    ");
  Serial.println("------------------------------");
  Serial.println(" ");
  Serial.println("Aperte o botao para calibrar sem peso");

  //obtem counts de peso minimo
  AguardaSoltarBotaoCal();  
  AguardaApertarBotaoCal();
  CountsPesoMinimo = CalibraPesoMinimoOuMaximo();
  AguardaSoltarBotaoCal(); 
  
  Serial.println("Aperte o botao para calibrar com peso (5000g)");

  //obtem counts de peso maximo
  AguardaSoltarBotaoCal();  
  AguardaApertarBotaoCal();
  CountsPesoMaximo = CalibraPesoMinimoOuMaximo();  
  AguardaSoltarBotaoCal(); 
  
  //se os counts de peso minimo foram maiores que os de peso maximo, a calibração é invalida.
  if (CountsPesoMinimo >= CountsPesoMaximo)
  {
    Serial.println("[Erro] counts de peso minimo maiores que counts de peso maximo");
    AguardaApertarBotaoCal();
    AguardaSoltarBotaoCal();
    return NAO;
  }
  
  //se chegou aqui, os counts de adc de peso mínimo e maximo são validos.
  //os parametros de calibração são calculados e salvos.
  InfoPesagem.CalibracaoBalanca.CountsPesoMinimo = CountsPesoMinimo;
  InfoPesagem.CalibracaoBalanca.CountsPesoMaximo = CountsPesoMaximo;
  InfoPesagem.CalibracaoBalanca.FlagBalancaCalibrada = SIM;

  //calcular coeficientes da reta. 
  //Equação da reta:
  // Peso = InfoPesagem.CalibracaoBalanca.CoefAngular*Counts_ADC + InfoPesagem.CalibracaoBalanca.CoefLinear  [g]
  //
  //Usar site https://www.geogebra.org/m/MH5e3JY4 como referência
  //
  InfoPesagem.CalibracaoBalanca.CoefAngular = ( (PESO_MAXIMO_CALIBRACAO - PESO_MINIMO_CALIBRACAO) / (CountsPesoMaximo - CountsPesoMinimo));
  InfoPesagem.CalibracaoBalanca.CoefLinear = (-1*InfoPesagem.CalibracaoBalanca.CoefAngular*CountsPesoMinimo);
  SalvaCalibracao();

  Serial.println("Calibracao realizada!");
  Serial.print("Coeficiente linear: ");
  Serial.println(InfoPesagem.CalibracaoBalanca.CoefLinear);
  Serial.println("Coeficiente angular: ");
  Serial.print(InfoPesagem.CalibracaoBalanca.CoefAngular);
  
  Serial.println("[OK] calibracao feita e salva com sucesso");
  AguardaApertarBotaoCal();
  AguardaSoltarBotaoCal();  
  return SIM;
}

//Função: calibra peso minimo ou maximo
//Parametros: nenhum
//Retorno: media da leitura de counts do peso minimo ou maximo
int CalibraPesoMinimoOuMaximo(void)
{
  unsigned long SomaLeituras;
  int MediaLeituras;
  int ContadorAmostras;
  int UltimaLeituraADC;
  int LeituraAtualADC;
  
  SomaLeituras = 0;
  ContadorAmostras = NUMERO_LEITURAS_COUNTS_CALIBRACAO;
  UltimaLeituraADC = FazLeituraADC();

  //faz 'NUMERO_LEITURAS_COUNTS_CALIBRACAO' leituras
  do 
  {   
    ESP.wdtFeed(); 
    Serial.print("[CALIBRACAO] Amostra ");
    Serial.print(100-ContadorAmostras);
    Serial.println("/100");
    LeituraAtualADC = FazLeituraADC();

    if ( ((LeituraAtualADC + JANELA_COUNTS_CALIBRACAO) > UltimaLeituraADC) && ((LeituraAtualADC - JANELA_COUNTS_CALIBRACAO) < UltimaLeituraADC) )
    {
      ContadorAmostras--;
      SomaLeituras = SomaLeituras + LeituraAtualADC;
    }
    else
    {
      SomaLeituras = 0;
      ContadorAmostras = NUMERO_LEITURAS_COUNTS_CALIBRACAO;  
      UltimaLeituraADC = FazLeituraADC(); 
    }    
  }while(ContadorAmostras > 0);

  MediaLeituras = (int)(SomaLeituras/NUMERO_LEITURAS_COUNTS_CALIBRACAO);
  return MediaLeituras;
}

//Função: salva calibracao
//Parametros: nenhum
//Retorno: nenhum
void SalvaCalibracao(void) 
{
  char i;
  unsigned char * ptrCal;
  
  EEPROM.begin(sizeof(TCalibracao));
  ptrCal = (unsigned char *)&InfoPesagem.CalibracaoBalanca;
 
  for(i=0; i < sizeof(TCalibracao); i++)
  {
    EEPROM.write(ENDERECO_EEPROM_EMULADA+i, *ptrCal);  
    ptrCal++;
  }
  
  EEPROM.end();
  Serial.println("[OK] Calibracao salva!");
}

//Função: le calibracao previamente salva
//Parametros: nenhum
//Retorno: nenhum
void LeCalibracao(void) 
{
  char i;
  unsigned char * ptrCal;
  
  //Le e calcula checksum. Se nao corresponder ao lido, seta default de calibração
  EEPROM.begin(sizeof(TCalibracao));
  ptrCal = (unsigned char *)&InfoPesagem.CalibracaoBalanca;
  
  for(i=0; i < sizeof(TCalibracao); i++)
  {
    *ptrCal = EEPROM.read(ENDERECO_EEPROM_EMULADA+i);
    ptrCal++;
  }
   
  EnviaSerialInfosCalibracao();
  EEPROM.end();  
}

//Função: mostra informações da calibracao corrente
//Parametros: nenhum
//Retorno: nenhum
void EnviaSerialInfosCalibracao(void)
{
  Serial.println("Dados da calibracao lida: ");
  Serial.print("Flag de balanca calibrada: ");
  Serial.println(InfoPesagem.CalibracaoBalanca.FlagBalancaCalibrada);
  Serial.print("Coeficiente angular: ");
  Serial.println(InfoPesagem.CalibracaoBalanca.CoefAngular);
  Serial.print("Coeficiente linear: ");
  Serial.println(InfoPesagem.CalibracaoBalanca.CoefLinear);
  Serial.print("Counts ADC - Peso minimo: ");
  Serial.println(InfoPesagem.CalibracaoBalanca.CountsPesoMinimo);
  Serial.print("Counts ADC - Peso maximo: ");
  Serial.println(InfoPesagem.CalibracaoBalanca.CountsPesoMaximo);
}

//Função: inicializa e conecta-se na rede WI-FI desejada
//Parâmetros: nenhum
//Retorno: nenhum
void InitWiFi(void) 
{
    delay(10);
    Serial.println("------Conexao WI-FI------");
    Serial.print("Conectando-se na rede: ");
    Serial.println(SSID);
    Serial.println("Aguarde");
    
    ReconectaWiFi();
}
 
//Função: inicializa parâmetros de conexão MQTT(endereço do 
//        broker, porta e seta função de callback)
//Parâmetros: nenhum
//Retorno: nenhum
void InitMQTT(void) 
{
    MQTT.setServer(BROKER_MQTT, BROKER_PORT);   //informa qual broker e porta deve ser conectado
    MQTT.setCallback(MQTTCallback);            //atribui função de callback (função chamada quando qualquer informação de um dos tópicos subescritos chega)
}
 
//Função: função de callback do MQTT
//Parâmetros: nenhum
//Retorno: nenhum
void MQTTCallback(char* topic, byte* payload, unsigned int length) 
{   
    String msg;
 
    //obtem a string do payload recebido
    for(int i = 0; i < length; i++) 
    {
       char c = (char)payload[i];
       msg += c;
    }
 
    //verifica se o comando recebido foi para acionar botao virtual de confirmação de pesagem
    if (msg.equals("P"))
    {
       Serial.println("[MQTT] Recebido comando remoto de pesagem / acionamento do botão virtual de confirmacao de pesagem");
       AcionamentoBotaoVirtualPesagem = SIM;
       return;
    }

    //verifica se o comando recebido foi para configurar um valor de preço/kg
    if (msg.charAt(0) == 'V')
    {
      Serial.println("[MQTT] Recebido valor de preco / kg");
      InfoPesagem.ValorPorKG = msg.substring(1).toFloat();
      return;   
    }

    //verifica se o comando recebido foi para configurar um valor limite de tempo de funcionamento (para manutenção preventiva)
    if (msg.charAt(0) == 'T')
    {
      Serial.println("[MQTT] Recebido valor de tempo de funcionamento (demonstracao)");
      TempoDeFuncionamento_Demonstracao = msg.substring(1).toInt();
      IniciaContagemTempoManutencaoPreventiva(TempoDeFuncionamento_Demonstracao);
      return;   
    }
 }
 
//Função: reconecta-se ao broker MQTT (caso ainda não esteja conectado ou em caso de a conexão cair)
//        em caso de sucesso na conexão ou reconexão, o subscribe dos tópicos é refeito.
//Parâmetros: nenhum
//Retorno: nenhum
void ReconectaMQTT(void) 
{
    while (!MQTT.connected()) 
    {
        Serial.print("* Tentando se conectar ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT)) 
        {
            Serial.println("Conectado com sucesso ao broker MQTT!");
            MQTT.subscribe(TOPICO_SUBSCRIBE); 
        } 
        else 
        {
            Serial.println("Falha ao reconectar no broker.");
            Serial.println("Havera nova tentatica de conexao em 2s");
            delay(2000);
        }
    }
}
 
//Função: reconecta-se ao WiFi
//Parâmetros: nenhum
//Retorno: nenhum
void ReconectaWiFi(void) 
{
    //se já está conectado a rede WI-FI, nada é feito. 
    //Caso contrário, são efetuadas tentativas de conexão
    if (WiFi.status() == WL_CONNECTED)
        return;
        
    WiFi.begin(SSID, PASSWORD); // Conecta na rede WI-FI
    
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(100);
        Serial.print(".");
        ESP.wdtFeed();
    }
  
    Serial.println();
    Serial.print("Conectado com sucesso na rede ");
    Serial.print(SSID);
    Serial.println("IP obtido: ");
    Serial.println(WiFi.localIP());
}
 
//Função: verifica o estado das conexões WiFI e ao broker MQTT. 
//        Em caso de desconexão (qualquer uma das duas), a conexão
//        é refeita.
//Parâmetros: nenhum
//Retorno: nenhum
void VerificaConexoesWiFIEMQTT(void)
{
    if (!MQTT.connected()) 
        ReconectaMQTT(); //se não há conexão com o Broker, a conexão é refeita
    
    ReconectaWiFi(); //se não há conexão com o WiFI, a conexão é refeita
}

//Função: verifica se é o momento de enviar dados via MQTT
//Parâmetros: nenhum
//Retorno: nenhum
void VerificaSeDeveEnviarDadosMQTT(void)
{
    char PesoEPrecoASCII[40];
    String PesoString;
    String ValorTotalString;
    String ValorPorKGString;
    char StringManutencaoPreventiva[4];
        
    if (TempoDecorrido(UltimaTemporizacao_EnvioMQTT) < TEMPO_ENVIO_MQTT)
      return;

    //valores convertidos para string
    PesoString = String(InfoPesagem.PesoKG,2);
    ValorTotalString = String(InfoPesagem.ValorTotalPesagem,2);
    ValorPorKGString = String(InfoPesagem.ValorPorKG,2);
            
    //publica o peso e preço total da pesagem via MQTT
    memset(PesoEPrecoASCII,0,sizeof(PesoEPrecoASCII));

    //verifica se deve enviar alerta de manutenção preventiva
    switch (VerificaSeGeraAlertaManutencaoPreventiva())
    {
      case SIM: 
        sprintf(StringManutencaoPreventiva,"SIM");
        break;

      case NAO: 
        sprintf(StringManutencaoPreventiva,"NAO");
        break;

      case NAO_APLICAVEL: 
        sprintf(StringManutencaoPreventiva,"SEM");
        break;
    }

    //formata a mensagem a ser enviada
    sprintf(PesoEPrecoASCII, "%s-%s-%s-%c-%c-%c-%s", PesoString.c_str(), 
                                                     ValorTotalString.c_str(), 
                                                     ValorPorKGString.c_str(),
                                                     InfoPesagem.FlagPlataformaZerada+0x30, 
                                                     InfoPesagem.FlagPlataformaEstavel+0x30, 
                                                     InfoPesagem.CalibracaoBalanca.FlagBalancaCalibrada+0x30,
                                                     StringManutencaoPreventiva);
                                                    
    MQTT.publish(TOPICO_PUBLISH, PesoEPrecoASCII);
    UltimaTemporizacao_EnvioMQTT = millis();
}

//Função: confirma pesagem (acumula preço total e peso e envia informações para ThingSpeak)
//Parâmetros: nenhum
//Retorno: nenhum
void ConfirmaPesagem(void)
{
  InfoPesagem.PesoAcumuladoKG = InfoPesagem.PesoAcumuladoKG + InfoPesagem.PesoKG;
  InfoPesagem.PrecoAcumulado = InfoPesagem.PrecoAcumulado + InfoPesagem.ValorTotalPesagem;
  EnviaInformacoesPesagemConfirmadaThingspeak(InfoPesagem.PesoKG, InfoPesagem.ValorTotalPesagem); 
}

//Função: envia informações de pesagem confirmada ao ThingSpeak
//Parâmetros: strings a serem enviadas (field1 = preço total; field2; peso total
//Retorno: nenhum
void EnviaInformacoesPesagemConfirmadaThingspeak(float DadosField1, float DadosField2)
{
  char StringTotal[40];
  String ValConvertido1;
  String ValConvertido2;
  WiFiClient clientHTTP;

  if (TempoDecorrido(UltimaTemporizacao_EnvioThingSpeak_Pesagens) < TEMPO_ENVIO_FUNCIONAMENTO_THINGSPEAK_PESAGENS)
    return;

  if (clientHTTP.connect(EnderecoAPIThingSpeak, 80))
  {         
    ValConvertido1 = String(DadosField1,2);
    ValConvertido2 = String(DadosField2,2);


    sprintf(StringTotal,"field1=%s&field2=%s",ValConvertido1.c_str(),ValConvertido2.c_str());
    
    //faz a requisição HTTP ao ThingSpeak
    clientHTTP.print("POST /update HTTP/1.1\n");
    clientHTTP.print("Host: api.thingspeak.com\n");
    clientHTTP.print("Connection: close\n");
    clientHTTP.print("X-THINGSPEAKAPIKEY: "+ChaveEscritaThingSpeak_PesagensConfirmadas+"\n");
    clientHTTP.print("Content-Type: application/x-www-form-urlencoded\n");
    clientHTTP.print("Content-Length: ");
    clientHTTP.print(strlen(StringTotal));
    clientHTTP.print("\n\n");
    clientHTTP.print(StringTotal);

    //atualiza temporização do último envio ao ThingSpeak
    UltimaTemporizacao_EnvioThingSpeak_Pesagens = millis();
  }     
}

//Função: verifica se deve enviar informações de tempo de funcionamento ao ThingSpeak
//Parâmetros: nenhum
//Retorno: nenhum
void VerificaEnvioKeepAliveFuncionamentoThingspeak(void)
{
  char StringTotal[40];
  WiFiClient clientHTTP;
  
  if (TempoDecorrido(UltimaTemporizacao_EnvioThingSpeak_Funcionamento) < TEMPO_ENVIO_FUNCIONAMENTO_THINGSPEAK_FUNCIONAMENTO)
    return;

  if (clientHTTP.connect(EnderecoAPIThingSpeak, 80))
  {         
    sprintf(StringTotal,"field1=1");
    
    //faz a requisição HTTP ao ThingSpeak
    clientHTTP.print("POST /update HTTP/1.1\n");
    clientHTTP.print("Host: api.thingspeak.com\n");
    clientHTTP.print("Connection: close\n");
    clientHTTP.print("X-THINGSPEAKAPIKEY: "+ChaveEscritaThingSpeak_Funcionamento+"\n");
    clientHTTP.print("Content-Type: application/x-www-form-urlencoded\n");
    clientHTTP.print("Content-Length: ");
    clientHTTP.print(strlen(StringTotal));
    clientHTTP.print("\n\n");
    clientHTTP.print(StringTotal);
  }     

  //atualiza temporização do último envio ao ThingSpeak
  UltimaTemporizacao_EnvioThingSpeak_Funcionamento = millis();  
}

//Função: imprime no serial monitor mensagens de debug de pesagem
//Parâmetros: nenhum
//Retorno: nenhum
void EscreveMensagemDebugPesagem(void)
{
    Serial.print("[PESO] ");
    Serial.print(InfoPesagem.PesoKG);
    Serial.print("g / ");

    Serial.print("[VALOR TOTAL] R$");
    Serial.print(InfoPesagem.ValorTotalPesagem);
 
    Serial.print(" / [VALOR POR KG] R$");
    Serial.print(InfoPesagem.ValorPorKG);
    Serial.println("/kg"); 
}

//Função: inicia a contagem do tempo de funcionamento (para demonstracao de manuenção preventiva)
//Parâmetros: tempo limite e funcionamento para gerr alerta
//Retorno: nenhum
void IniciaContagemTempoManutencaoPreventiva(unsigned long TempoLimite)
{
  TempoDeFuncionamento_Demonstracao = TempoLimite*1000;
  UltimaTemporizacao_FuncionamentoBalanca = millis();
}

//Função: verifica se deve gera alerta de manutenção preventiva
//Parâmetros: nenhum
//Retorno: SIM: deve gerar alerta
//         NAO: não deve gerar alerta
//         NAO_APLICAVEL: nao ha alerta configurado
char VerificaSeGeraAlertaManutencaoPreventiva(void)
{
  if (TempoDeFuncionamento_Demonstracao != 0)
  {
    if (TempoDecorrido(UltimaTemporizacao_FuncionamentoBalanca) > TempoDeFuncionamento_Demonstracao)
      return SIM;
    else
      return NAO;  
  }
  else
      return NAO_APLICAVEL;
}

void setup() {
  //inicialização da serial para debug / minitoramento
  Serial.begin(BAUDRATE_SERIAL_MONITOR);

  //inicializa varíável de informações de pesagem
  memset(&InfoPesagem,0,sizeof(TPesagem));

  //configura pinos de input e output
  pinMode(BREATHING_LIGHT, OUTPUT);
  pinMode(BOTAO_CALIBRAR, INPUT_PULLUP);
  pinMode(LED_INDICATIVO_BALANCA_CALIBRADA, OUTPUT);
  digitalWrite(BREATHING_LIGHT, LOW);
  digitalWrite(LED_INDICATIVO_BALANCA_CALIBRADA, LOW);

  //inicialização de variaveis globais gerais
  UltimaTemporizacao_BreatlingLight = millis();
  UltimaTemporizacao_LeituraADC = millis();
  UltimaTemporizacao_EnvioMQTT = millis();
  UltimaTemporizacao_EnvioThingSpeak_Funcionamento = millis();
  UltimaTemporizacao_EnvioThingSpeak_Pesagens = millis();
  TempoDeFuncionamento_Demonstracao = 0;
  EstadoBreathingLight = LED_ACESO;
  PesoAnterior = 0.0;
  InfoPesagem.ValorPorKG = 0.0;
  AcionamentoBotaoVirtualPesagem = NAO;
  TempoEntreConversoesADC = (int)((float)(1/NUMERO_AMOSTRAS_ADC_POR_SEGUNDO)*1000.0);

  //le calibração atual
  LeCalibracao();  

  //inicializações de rede
  InitWiFi();
  InitMQTT();
}

/*
 * Programa principal
 */
void loop() 
{   
  //garante funcionamento das conexões WiFi e ao broker MQTT
  VerificaConexoesWiFIEMQTT();
  
  //verifica se é hora de obter conversão de ADC e gerar pesagem
  if (TempoDecorrido(UltimaTemporizacao_LeituraADC) >= TempoEntreConversoesADC)
  {
    #ifndef FAZ_SOMENTE_LEITURA_ADC
      LeADCEFazPesagem();
      CalculaPrecoPesagem();
      EscreveMensagemDebugPesagem();
      VerificaSeDeveEnviarDadosMQTT();      
    #else  
      Serial.println(FazLeituraADC());
    #endif  
  }

  #ifndef FAZ_SOMENTE_LEITURA_ADC
    //verifica se o botão de calibração (fisico ou virtual) foi apertado
    if (VerificaBotaoCalibrar() == SIM)    
      FazCalibracao();
      
   //verifica se o botão virtual de confirmação de pesagem foi apertado   
   VerificaBotaoVirtualConfirmacaoPesagem();   
 #endif    

  //faz com que o beathing light pisque a 1Hz
  if (TempoDecorrido(UltimaTemporizacao_BreatlingLight) >= TEMPO_BREATHINGLIGHT)
    ToogleBreathingLight();

  //verifica se deve enviar informação de tempo de funcionamento da balança ao ThingSpeak
  VerificaEnvioKeepAliveFuncionamentoThingspeak();

  //keep-alive da comunicação com broker MQTT
  MQTT.loop();

  //restarta WDT
  ESP.wdtFeed();   
}
