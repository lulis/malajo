// MALAJO - Máquina de Lavar Roupas Ecoeficiente do Jovim
// v0.8.2.1
#include <Servo.h>

//// TINKERCAD_MODE ////
// Se definido, usa melhorias para debug no tinkercad
// COMENTAR com "//" a linha abaixo PARA USAR NO ARDUINO !!!
#define TINKERCAD_MODE

// ALERTA !!!
// Pin 0 (PET) na simulacao tinkercad fica sempre ON!!!
// Talvez seja limitacao da Serial o uso dos pinos 0 e 1


//// CONSTANTES ////

/// Saidas Digitais
// Saidas NF/NA (Normalmente Aberta) tratadas em "mudaEstado"
// valvula saida reservatorio reuso (NF)
// TODO: seria mais "natural" a associacao se fosse "1"
const int v1_saida_reuso = 13;
// valvula saida reservatorio chuva (NF)
const int v2_saida_chuva = 2;
// valvula saida descarte para tanque (NA)
const int v3_descarte_tanque = 3;
// valvula entrada reservatorio reuso (NF)
const int v4_entrada_reuso = 4;
// valvula entrada agua limpa (NA)
const int v5_entrada_tratada = 5;
// eletrobomba saida reservatorios
const int eb_reservatorios = 6;

// Servomotores associados às valvulas
Servo servo_v1;
Servo servo_v2;
Servo servo_v3;
Servo servo_v4;
Servo servo_v5;
// Servo rotacional
// 0 = full-speed horario, ~90 = parado, 180 = full-speed anti-horario
int SERVO_ABRINDO = 0;
int SERVO_PARADO = 90;
int SERVO_FECHANDO = 180;
// tempo (ms) de atividade do servo para completar o curso da valvula
unsigned long tempo_curso_valvula = 750;


/// Entradas Digitais
// sensor valvula dispenser MLR - Sabão
const int sv7_sabao = 7;
// sensor valvula dispenser MLR - Amaciante
const int sv8_amacia = 8;
// sensor valvula eletrobomba descarga MLR
const int seb_descarga = 9;
// sensor micro chave tampa MLR
const int smc_tampa = 11;
// array com todas entradas
const int pinos_entradas[] = { sv7_sabao, sv8_amacia, seb_descarga, smc_tampa};

/// Outros
// botao selecao modo uso agua da chuva
const int btn_chuva = 10;
// botao selecao modo uso agua de reuso
const int btn_reuso = 1;
// botao selecao modo PET (roupa muito suja, nao reusar agua)
const int btn_pet_mode = 12;
// led indicacao estado atual (pin 13 = LED_BUILTIN)
const int led_estado = 0;
// tempo (ms) de agua limpa pro dispenser
unsigned long tempo_dispenser = 60000;
// tempo (ms) de espera entre troca de estados
unsigned long tempo_espera_estado = 500;
// tempo (ms) minimo de persistencia em uma mudanca na entrada
// para considerar valido proceder com a troca de estado
unsigned long tempo_minimo_entrada = 2000;
// tempo (ms) de espera apos fechamento da tampa
unsigned long tempo_espera_tampa = 6000;

//// VARIAVEIS ////
// Configuracao inicial em "resetConfig"
int estado_atual;
unsigned long ultima_troca_estado;
unsigned long ultimo_tempo_tampa;
unsigned long ultima_alteracao_entrada;
bool eh_pre_enxague;
bool tampa_estava_aberta;
bool avaliando_entrada;


// CONFIGURACAO
void setup()
{
  #ifdef TINKERCAD_MODE
    // reduzimos os tempos pra ficar viavel a simulacao
    tempo_dispenser = 750;
    tempo_espera_estado = 250;
    tempo_minimo_entrada = 250;
    tempo_espera_tampa = 500;
    // usamos angulos curtos nos servos, que nos ajudam a ver
    SERVO_ABRINDO = 150;
    SERVO_PARADO = 90;
    SERVO_FECHANDO = 30;
    tempo_curso_valvula = 1500;
  #endif

  // Saidas Digitais
  servo_v1.attach(v1_saida_reuso);
  servo_v2.attach(v2_saida_chuva);
  servo_v3.attach(v3_descarte_tanque);
  servo_v4.attach(v4_entrada_reuso);
  servo_v5.attach(v5_entrada_tratada);
  pinMode(eb_reservatorios, OUTPUT);

  // Entradas Digitais
  for (int i = 0; i < (sizeof(pinos_entradas)/sizeof(pinos_entradas[0])); i++) {
    pinMode(pinos_entradas[i], INPUT);
  }
  // Outros
  pinMode(btn_chuva, INPUT);
  pinMode(btn_reuso, INPUT);
  pinMode(btn_pet_mode, INPUT);
  pinMode(led_estado, OUTPUT);
  reset_config();
}

// Configuracao inicial padrao
void reset_config() {
  eh_pre_enxague = false;
  tampa_estava_aberta = false;
  avaliando_entrada = false;
  desativa_saidas();
  troca_estado(1);
  // Para evitar flutuacoes ao ligar. Talvez seja dispensavel
  // dado que implementamos esperando_tempo_minimo_entrada()
  delay(tempo_minimo_entrada);
}

// Desativa saidas mimetizando comportamento usual da MLR
void desativa_saidas() {
  desliga(eb_reservatorios);
  desliga(v1_saida_reuso);
  desliga(v2_saida_chuva);
  liga(v3_descarte_tanque);
  desliga(v4_entrada_reuso);
  liga(v5_entrada_tratada);
}


//// EXECUCAO ////
void loop() {

  trata_led_estado();

  // trata tampa aberta + tempo de espera
  if ( ! ta_ligada(smc_tampa) ) {
    ultimo_tempo_tampa = millis();
    if ( ! tampa_estava_aberta ) {
      tampa_estava_aberta = true;
      inibe_saidas();
    }
    return;
  }
  else if ( tampa_estava_aberta ) {
    if ( millis() - tempo_espera_tampa >= ultimo_tempo_tampa ) {
      tampa_estava_aberta = false;
      restaura_saidas();
    }
    return;
  }

  // trata cada um dos estados
  switch (estado_atual) {

  // ML Desligada
  case 1:
    if ( ta_ligada(sv7_sabao) ) {
      if ( esperando_tempo_minimo_entrada()) return;
      // se puder usar reuso ou chuva
      if ( ta_ligada(btn_reuso) || ta_ligada(btn_chuva) ) {
        // usa preferencialmente reuso, se nao, usa chuva
        if ( ta_ligada(btn_reuso) ) {
          liga(v1_saida_reuso);
        } else {
          liga(v2_saida_chuva);
        }
        desliga(v3_descarte_tanque);
        liga(eb_reservatorios);
        troca_estado(2);
      }
      // se nao, usa apenas agua limpa
      else {
        troca_estado(4);
      }
    }
    break;

  // ML Enchendo Lavagem + Dispenser
  case 2:
    if ( tempo_ultima_troca_estado() >= tempo_dispenser ) {
      desliga(v5_entrada_tratada);
      troca_estado(3);
    }
    break;

  // ML Enchendo Lavagem
  case 3:
    //~ reavalia_entrada_agua(); TODO
    if ( ! ta_ligada(sv7_sabao) ) {
      if ( esperando_tempo_minimo_entrada()) return;
      desliga(eb_reservatorios);
      // desliga saidas reuso ou chuva se precisar (nao forca servo)
      if ( ta_ligada(v1_saida_reuso) ) desliga(v1_saida_reuso);
      if ( ta_ligada(v2_saida_chuva) ) desliga(v2_saida_chuva);
      liga(v3_descarte_tanque);
      liga(v5_entrada_tratada);
      troca_estado(4);
    }
    break;

  // ML Lavando
  case 4:
    if ( ta_ligada(seb_descarga) ) {
      if ( esperando_tempo_minimo_entrada()) return;
      troca_estado(5);
    }
    // Caso altere nivel da ML e comece re-encher
    else if ( ta_ligada(sv7_sabao) ) {
      if ( esperando_tempo_minimo_entrada()) return;
      if ( ta_ligada(btn_reuso) || ta_ligada(btn_chuva) ) {
        // usa preferencialmente reuso, se nao, usa chuva
        if ( ta_ligada(btn_reuso) ) {
          liga(v1_saida_reuso);
        } else {
          liga(v2_saida_chuva);
        }
        desliga(v3_descarte_tanque);
        liga(v5_entrada_tratada);
        liga(eb_reservatorios);
        troca_estado(2);
      }
    }
    break;

  // ML Esvaziando Lavagem
  case 5:
    if ( ! ta_ligada(seb_descarga) ) {
      if ( esperando_tempo_minimo_entrada()) return;
      desliga(v3_descarte_tanque);
      troca_estado(6);
    }
    break;

  // ML Prepara Enxague
  case 6:
    if ( ta_ligada(sv7_sabao) || ta_ligada(sv8_amacia) ) {
      if ( esperando_tempo_minimo_entrada()) return;
      // se for sabao, serao 2 enxagues
      eh_pre_enxague = ta_ligada(sv7_sabao);
      // agua chuva
      if ( ta_ligada(btn_chuva) ) {
        liga(v2_saida_chuva);
        liga(eb_reservatorios);
        troca_estado(7);
      }
      // agua limpa
      else {
        troca_estado(9);
      }
    }
    break;

  // ML Enchendo Enxague + Dispenser
  case 7:
    if ( tempo_ultima_troca_estado() >= tempo_dispenser ) {
      desliga(v5_entrada_tratada);
      troca_estado(8);
    }
    break;

  // ML Enchendo Enxague
  case 8:
    if ( (eh_pre_enxague && !ta_ligada(sv7_sabao)) ||
      (!eh_pre_enxague && !ta_ligada(sv8_amacia)) ) {
      if ( esperando_tempo_minimo_entrada()) return;
      desliga(eb_reservatorios);
      desliga(v2_saida_chuva);
      liga(v3_descarte_tanque);
      liga(v5_entrada_tratada);
      troca_estado(9);
    }
    break;

  // ML Enxaguando
  case 9:
    if ( ta_ligada(seb_descarga) ) {
      if ( esperando_tempo_minimo_entrada()) return;
      // modo salvar pra reuso (nao pet)
      if ( !ta_ligada(btn_pet_mode) ) {
        desliga(v3_descarte_tanque);
        liga(v4_entrada_reuso);
      }
      troca_estado(10);
    }
    break;

  // ML Esvaziando Prenxague
  case 10:
    if ( ! ta_ligada(seb_descarga) ) {
      if ( esperando_tempo_minimo_entrada()) return;
      if ( eh_pre_enxague ) {
        // desliga entrada reuso se precisar (nao forca servo)
        if ( ta_ligada(v4_entrada_reuso) ) desliga(v4_entrada_reuso);
        troca_estado(6);
      }
      else {
        reset_config(); // volta estado 1
      }
    }
    break;

  // Estado desconhecido, reiniciamos
  default:
    reset_config();
    break;
  }

  // Esse cara garante que esperando_tempo_minimo_entrada()
  // funcione entre falsos positivos seguidos nas entradas
  reinicia_tempo_minimo_entrada();
}


// Funcoes auxiliares de ESTADO
void troca_estado(int novo_estado) {
  delay(tempo_espera_estado);
  estado_atual = novo_estado;
  ultima_troca_estado = millis();
  reinicia_led_estado();
}
unsigned long tempo_ultima_troca_estado() {
  return millis() - ultima_troca_estado;
}
bool esperando_tempo_minimo_entrada() {
  if (!avaliando_entrada) {
    avaliando_entrada = true;
    ultima_alteracao_entrada = millis();
  }
  else if ( millis() - ultima_alteracao_entrada >= tempo_espera_estado ) {
    reinicia_tempo_minimo_entrada();
  }
  return avaliando_entrada;
}
void reinicia_tempo_minimo_entrada(){
  if (avaliando_entrada) {
    avaliando_entrada = false;
  }
}

// Funcoes auxiliares de IN/OUT
bool saida_salva_eb, saida_salva_v1, saida_salva_v2, saida_salva_v3, saida_salva_v4, saida_salva_v5;
bool ta_ligada(int entrada) {
  switch (entrada) {
    case v1_saida_reuso:
      return saida_salva_v1;
    case v2_saida_chuva:
      return saida_salva_v2;
    case v3_descarte_tanque:
      return saida_salva_v3;
    case v4_entrada_reuso:
      return saida_salva_v4;
    case v5_entrada_tratada:
      return saida_salva_v5;
    default:
      break;
  }
  return digitalRead(entrada) == HIGH;
}
void liga(int saida) {
  muda_saida(saida, true);
}
void desliga(int saida) {
  muda_saida(saida, false);
}
void muda_saida(int saida, bool valor) {
  // Saidas Rele
  if (saida ==  eb_reservatorios) {
    // Rele funciona invertido, usamos "1-valor"
    digitalWrite(saida, 1 - valor);
    if (!tampa_estava_aberta) saida_salva_eb = valor;
  }
  // Saidas Servomotor
  else {
    int velocidade = valor ? SERVO_ABRINDO : SERVO_FECHANDO;
    switch (saida) {
    case v1_saida_reuso:
      if (!tampa_estava_aberta) saida_salva_v1 = valor;
      servo_v1.write(velocidade);
      delay(tempo_curso_valvula);
      servo_v1.write(SERVO_PARADO);
      break;
    case v2_saida_chuva:
      if (!tampa_estava_aberta) saida_salva_v2 = valor;
      servo_v2.write(velocidade);
      delay(tempo_curso_valvula);
      servo_v2.write(SERVO_PARADO);
      break;
    case v3_descarte_tanque:
      if (!tampa_estava_aberta) saida_salva_v3 = valor;
      servo_v3.write(velocidade);
      delay(tempo_curso_valvula * 1.33);
      servo_v3.write(SERVO_PARADO);
      break;
    case v4_entrada_reuso:
      if (!tampa_estava_aberta) saida_salva_v4 = valor;
      servo_v4.write(velocidade);
      delay(tempo_curso_valvula);
      servo_v4.write(SERVO_PARADO);
      break;
    case v5_entrada_tratada:
      if (!tampa_estava_aberta) saida_salva_v5 = valor;
      servo_v5.write(velocidade);
      delay(tempo_curso_valvula);
      servo_v5.write(SERVO_PARADO);
      break;
    }
  }
}

// Variaveis e funcoes EXCLUSIVAS
// Para manipulacao de portas de saida
// Usando Servo Rotacional (preserva posicao da valvula), tratamos apenas Reles
void inibe_saidas() {
  // Salvamento
  // - Implementado a cada alteracao, em "muda_saida"
  // Desativacao
  // - Estado inicial seria ideal, mas evitamos movimentacao inutil de valvulas joao
  //~ desativa_saidas();
  // Servomotores
  // - Apenas v1 e v2, por causa do efeito sifão, se ligados
  if ( ta_ligada(v1_saida_reuso) ) desliga(v1_saida_reuso);
  if ( ta_ligada(v2_saida_chuva) ) desliga(v2_saida_chuva);
  // Reles
  desliga(eb_reservatorios);
}
void restaura_saidas() {
  // Servomotores
  if ( ta_ligada(v1_saida_reuso) ) liga(v1_saida_reuso);
  if ( ta_ligada(v2_saida_chuva) ) liga(v2_saida_chuva);
  //~ muda_saida(v3_descarte_tanque, saida_salva_v3);
  //~ muda_saida(v4_entrada_reuso, saida_salva_v4);
  //~ muda_saida(v5_entrada_tratada, saida_salva_v5);
  // Reles
  muda_saida(eb_reservatorios, saida_salva_eb);
}


// Variaveis e funcoes EXCLUSIVAS
// Para logica de LED de estado atual
// Quase da pra debugar (ruim!) com osciloscopio 500ms
unsigned long previous_millis = 0;
bool led_status = false;
int piscadas = 0;
void trata_led_estado() {
  unsigned long current_millis = millis();
  if (current_millis >= previous_millis + 250) {
    led_status = 1 - led_status;
    if (led_status) piscadas++;
    if (piscadas > estado_atual) {
      previous_millis = current_millis + 1000;
      piscadas = 0;
      led_status = false;
    }
    else {
      previous_millis = current_millis;
    }
    digitalWrite(led_estado, led_status);
  }
}
void reinicia_led_estado() {
  piscadas = 999;
}