#ifndef CONFIGURATION_H
#define CONFIGURATION_H

// CONTANTES
#define WIFI_SSID                               "Aquapony"
#define WIFI_PASSWORD                           "12345678"
#define LOCAL_WIFI_SSID                         "SUELI"
#define LOCAL_WIFI_PASSWORD                     "santos1965"
#define WIFI_MAX_CONNECTIONS                    1
#define WIFI_DEFAULT_IP                         "192.168.0.4"
#define WIFI_CHANNEL                            1
#define SERVER_PORT                             80
#define SOCKET_URL                              "/ws"
#define EVENTS_URL                              "/events"

#define MIN_AQUARIUM_TEMP                       23
#define MAX_AQUARIUM_TEMP                       28
#define MIN_AQUARIUM_PH                         6
#define MAX_AQUARIUM_PH                         7

#define TAXA_DEFASAGEM_LOWER_SOLUTION_DKH       0.2
#define DOSAGE_LOWER_SOLUTION_M3_L              13

#define TAXA_DEFASAGEM_RAISE_SOLUTION_DKH       0.2
#define DOSAGE_RAISE_SOLUTION_M3_L              13

#define TAXA_DEFASAGEM_SEGURANCA_DKH            0.2
#define AQUARIUM_VOLUME_L                       10
#define DEFAULT_TIME_DELAY_PH                   24 * 60 * 60 * 1000
#define FLUXO_PERISTALTIC_ML_S                  100/60 

/*
==========================================================================================================================================

1 meq/l - 50.084 ppm
1 meq/l - 2.8 dKh

pH= pKa +log([Acido]/[Base])
dkH = (Vacido * conentracao * 50) / Volume amostra
dkH = (Vacido) / Volume amostra * 280
==========================================================================================================================================
Tabela de Dosagem do hth® Redutor de Alcalinidade e pH 1Lt
        Parâmetro	                    Dosagem
Alcalinidade acima de 120 ppm	            13 ml/m³
pH entre 7,4 e 8,0	                    13 ml/m³
pH acima de 8,0	                            25 ml/m³
Observação: Cada m³ equivale a 1.000 litros. Cada 13 ml/m³ diminui 10ppm de alcalinidade total e cada 13 ml/m³ diminui 0,2 de pH.

=========================================================================================================================================

alkaline Buffer reduz: 1 meq/l(2,8 dKH) (50.084 ppm)

==========================================================================================================================================
Molar solution equation: desired molarity × formula weight × solution final volume (L) = grams needed
Percentage by weight (w/v): (% buffer desired / 100) × final buffer volume (mL) = g of starting material needed
Henderson-Hasselbach equation: pH = pKa + log [A-]/[HA]
==========================================================================================================================================
CO2(ppm) = 3 * Kh * 10^(7 - ph)
Kh = CO2(ppm) / (3 * 10 ^(7 - ph))

CO2 - ppm
low   < 10
safe  10 > 25
high  > 25
==========================================================================================================================================

// Constantes
const double R = 8.314; // Constante dos gases ideais em J/(mol·K)
const double deltaH_sol = -24000; // Entalpia de solução em J/mol
const double kH_T0 = 3.3e-2; // Constante de Henry a T0 em mol/(L·atm)
const double T0 = 298; // Temperatura de referência em Kelvin (25°C)

// Função para calcular a constante de Henry a uma nova temperatura
double calcularConstanteHenry(double T) {
    double deltaT = (1 / T) - (1 / T0);
    double exponent = (-deltaH_sol / R) * deltaT;
    double kH_T = kH_T0 * std::exp(exponent);
    return kH_T;
}
double kH = calcularConstanteHenry(T);

T(em Kelvin)
*/



// INPUTS
#define PIN_BUTTON_RESET                                      5
#define PIN_THERMOCOUPLE                                      14        // PINO: SENSOR DE TEMPERATURA 
#define PIN_PH                                                35        // PINO: SENSOR DE PH
#define PIN_TURBIDITY                                         34        // PINO: SENSOR DE TURBIDADE

#define PIN_CLOCK_CLK                                         4
#define PIN_CLOCK_DAT                                         2
#define PIN_CLOCK_RST                                         15

// OUTPUTS
#define PIN_HEATER                                            22        // PINO: AQUECEDOR
#define PIN_WATER_PUMP                                        23        // PINO: BOMBA DE AGUA
#define PIN_COOLING                                           13        // NÃO IMPLEMENTADO
#define PIN_PERISTAULTIC                                      18        // NÃO IMPLEMENTADO



// ENDERÇOS DE MEMORIA EEMPROM  
#define EEPROM_SIZE                                           4096      // 4096U - 1U = 1 kB
#define ADDRESS_START                                         0x000000
#define ADDRESS_SSID                                          0x000001
#define ADDRESS_PASSWORD                                      0x000005
#define ADDRESS_AQUARIUM_TEMPERATURE                          0x000009
#define ADDRESS_AQUARIUM_MIN_TEMPERATURE                      0x00000D
#define ADDRESS_AQUARIUM_MAX_TEMPERATURE                      0x000011
#define ADDRESS_AQUARIUM_MIN_PH                               0x000015
#define ADDRESS_AQUARIUM_MAX_PH                               0x000019
#define ADDRESS_DOSAGEM_REDUTOR_PH                            0x00001D
#define ADDRESS_PPM_PH                                        0x000021
#define ADDRESS_LAST_APPLICATION_ACID_BUFFER_PH               0x000025
#define ADDRESS_CYCLE_TIME_WATER_PUMP                         0x000029
#define SIZE_ROUTINES                                         4000U



#endif