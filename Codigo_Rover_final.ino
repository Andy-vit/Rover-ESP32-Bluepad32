#include <Bluepad32.h>
#include <ESP32Servo.h>

ControllerPtr myControllers[BP32_MAX_GAMEPADS];
Servo miServo;

// ==========================================
// PINES DEL HARDWARE
// ==========================================
// Dirección
const int pinServo = 22;

// Tracción - Motor Izquierdo (Canal A)
const int ENA = 12;
const int IN1 = 26;
const int IN2 = 25;

// Tracción - Motor Derecho (Canal B)
const int ENB = 33;
const int IN3 = 27; 
const int IN4 = 14; 

// ==========================================
// CONFIGURACIÓN DEL SERVO
// ==========================================
const int anguloMin = 68;
const int anguloMax = 115;
const int anguloCentro = 91;

// ==========================================
// FUNCIONES DE BLUETOOTH (BLUEPAD32)
// ==========================================
void onConnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      myControllers[i] = ctl;
      Serial.println("🎮 ¡Mando GameSir conectado! A rodar...");
      return;
    }
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      myControllers[i] = nullptr;
      Serial.println("❌ Mando desconectado. FRENANDO POR SEGURIDAD.");
      return;
    }
  }
}

// ==========================================
// INICIO DEL SISTEMA (SETUP)
// ==========================================
void setup() {
  Serial.begin(115200);
  
  // 1. Configurar Motores L298N (Todos los pines como salida)
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  
  apagarMotores(); // Asegurar que inicie quieto

  // 2. Configurar Servo de Dirección
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  miServo.setPeriodHertz(50);
  miServo.attach(pinServo, 500, 2400);
  miServo.write(anguloCentro);

  // 3. Iniciar Bluetooth
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys();
  Serial.println("ESP32 Listo. Pon tu GameSir en modo vinculación (DS4)...");
}

// ==========================================
// BUCLE PRINCIPAL (LOOP)
// ==========================================
void loop() {
  BP32.update();
  bool hayMandoConectado = false;

  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    ControllerPtr ctl = myControllers[i];
    
    if (ctl && ctl->isConnected()) {
      hayMandoConectado = true;

      // ----------------------------------------
      // 1. CONTROL DE DIRECCIÓN (Joystick Izquierdo)
      // ----------------------------------------
      int joyX = ctl->axisX();
      
      // Zona Muerta: Ignoramos valores entre -15 y 15
      if (joyX > -15 && joyX < 15) {
        joyX = 0;
      }
      
      // Traducimos el Joystick al Servo (¡Matemática invertida para corregir el giro!)
      int angulo = map(joyX, -512, 512, anguloMax, anguloMin);
      miServo.write(angulo);

      // ----------------------------------------
      // 2. CONTROL DE TRACCIÓN (Gatillos L2 y R2)
      // ----------------------------------------
      int gatilloR2 = ctl->throttle(); // Acelerar (0 a 1020)
      int gatilloL2 = ctl->brake();    // Reversa (0 a 1020)

      // Zona muerta para los gatillos
      if (gatilloR2 < 10) gatilloR2 = 0;
      if (gatilloL2 < 10) gatilloL2 = 0;

      // Lógica de movimiento para los DOS motores simultáneamente
      if (gatilloR2 > 0 && gatilloL2 == 0) {
        // --- IR HACIA ADELANTE ---
        int velocidad = map(gatilloR2, 0, 1020, 0, 255); 
        
        // Motor Izquierdo
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        analogWrite(ENA, velocidad);
        
        // Motor Derecho
        digitalWrite(IN3, HIGH);
        digitalWrite(IN4, LOW);
        analogWrite(ENB, velocidad);
      } 
      else if (gatilloL2 > 0 && gatilloR2 == 0) {
        // --- IR HACIA ATRÁS ---
        int velocidad = map(gatilloL2, 0, 1020, 0, 255); 
        
        // Motor Izquierdo
        digitalWrite(IN1, LOW);
        digitalWrite(IN2, HIGH);
        analogWrite(ENA, velocidad);
        
        // Motor Derecho
        digitalWrite(IN3, LOW);
        digitalWrite(IN4, HIGH);
        analogWrite(ENB, velocidad);
      } 
      else {
        // --- FRENO TOTAL ---
        // (Si no presiona nada, o presiona AMBOS gatillos a la vez)
        apagarMotores();
      }
    }
  }

  // ----------------------------------------
  // SISTEMA DE SEGURIDAD (Hombre Muerto)
  // ----------------------------------------
  if (!hayMandoConectado) {
    apagarMotores();
    // Opcional: Centrar ruedas al perder señal
    // miServo.write(anguloCentro); 
  }
  
  // Un respiro para el procesador
  delay(20); 
}

// ==========================================
// FUNCIÓN AUXILIAR: APAGAR TODOS LOS MOTORES
// ==========================================
void apagarMotores() {
  // Apagar Motor Izquierdo
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 0);
  
  // Apagar Motor Derecho
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENB, 0);
}