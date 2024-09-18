#ifndef CONFIGURATION_H
#define CONFIGURATION_H
#include <algorithm>
// CONTANTES


#define MAX_SIZE                                static_cast<size_t>(60)
#define MAX_BUFFER_SIZE  35000

#define MIN_AQUARIUM_TEMP                       23
#define MAX_AQUARIUM_TEMP                       28
#define MIN_AQUARIUM_PH                         6
#define MAX_AQUARIUM_PH                         7

#define TAXA_DEFASAGEM_LOWER_SOLUTION_DKH       0.2
#define DOSAGE_LOWER_SOLUTION_ML_L              2

#define TAXA_DEFASAGEM_RAISE_SOLUTION_DKH       0.2
#define DOSAGE_RAISE_SOLUTION_ML_L              2

#define TAXA_DEFASAGEM_SEGURANCA_DKH            0.2
#define DELTA_MAXIMO_PH                         0.3

#define AQUARIUM_VOLUME_L                       10
#define DEFAULT_TIME_DELAY_PH                   30 // SEGUNDOS

#define CHANNEL_SOLUTION_LOWER                  0
#define CHANNEL_SOLUTION_RAISER                 1
#define POTENCIA_PERISTAULTIC                   40
#define FLUXO_PERISTALTIC_ML_S                  1.68



#define SERVICE_UUID                            "02be3f08-b74b-4038-aaa4-5020d1582eba"
#define CHARACTERISTIC_WIFI_UUID                "6fd27a35-0b8a-40cb-ad23-3f3f6c0d8626"
#define CHARACTERISTIC_INFO_UUID                "eeaaf2ad-5264-47a1-a49f-5b274ab1a0fe"
#define CHARACTERISTIC_SYSTEM_INFO_UUID         "26f8dc2f-213a-4048-a898-07eaf4f2089e"
#define CHARACTERISTIC_CONFIGURATION_UUID       "b371220d-3559-410d-8a47-78b06df6eb3a"
#define CHARACTERISTIC_RTC_UUID                 "a5939a1a-0b50-48d0-8d03-fad87790ab4a"
#define CHARACTERISTIC_PUMP_UUID                "da4a95ee-6998-40a6-ad8c-c87087e15ca6"
#define CHARACTERISTIC_GET_HIST_PH_UUID          "c496fa32-b11a-460b-8dd3-1aeeb7c7f0ae"
#define CHARACTERISTIC_GET_HIST_TEMP_UUID        "23ffac66-d0f9-4dbb-bb10-2b92d3664760"
#define CHARACTERISTIC_GET_HIST_APL_UUID         "d1b59240-370b-45eb-abf0-46968a5dcc7f"

#define SERVICE_ROUTINES_UUID                   "bb1db1b1-6696-4bfa-a140-8d5836e980c8"
#define CHARACTERISTIC_UPDATE_ROUTINES_UUID     "4b9c96d4-cf69-45ca-a157-9ac8d0b7b155"
#define CHARACTERISTIC_GET_ROUTINES_UUID        "7dc355e9-6089-4a6f-b22c-f65820cad8c0"
#define CHARACTERISTIC_DELETE_ROUTINES_UUID     "77fb44d3-2e17-4cec-9c22-e9a413c4eea7"
#define CHARACTERISTIC_CREATE_ROUTINES_UUID     "61b86e63-66bc-4b19-b178-5b5e92a67b9d"

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
#define PIN_RESET                                             22        // PINO: SENSOR DE TEMPERATURA 
#define PIN_THERMOCOUPLE                                      23        // PINO: SENSOR DE TEMPERATURA 
#define PIN_PH                                                35        // PINO: SENSOR DE PH
#define PIN_TURBIDITY                                         34        // PINO: SENSOR DE TURBIDADE

#define PIN_CLOCK_CLK                                         15
#define PIN_CLOCK_DAT                                         2
#define PIN_CLOCK_RST                                         4

// OUTPUTS
#define PIN_HEATER                                            5        // PINO: AQUECEDOR
#define PIN_WATER_PUMP                                        19        // PINO: BOMBA DE AGUA
#define PIN_PERISTAULTIC_RAISER                               13        // NÃO IMPLEMENTADO
#define PIN_PERISTAULTIC_LOWER                                12        // NÃO IMPLEMENTADO



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



#endif