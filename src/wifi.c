#include "wifi.h"

static bool wifi_conected=false;
static int num_of_tries=0; //static variable of number of tries to connected a net
static void wifi_event_handler ( void *event_handler_arg, esp_event_base_t base, 
    int32_t event_id, void *event_data)  
{ 
    
    //verify the conection to open 
    if (event_id == WIFI_EVENT_STA_START) 
    { 
        ESP_LOGI("R", "WIFI CONECTING....\n" ); 
        esp_wifi_connect();
        wifi_conected=false;
    } 
    else  if (event_id == WIFI_EVENT_STA_CONNECTED) 
    { 
        ESP_LOGI("R","WiFi CONECTED\n" ); 
        
    } 
    else  if (event_id == WIFI_EVENT_STA_DISCONNECTED) //if the conection are wrong try at least another 5 times 
    { 
        ESP_LOGI("R","wifi lost conection\n" ); 
        wifi_conected=false;
        if (num_of_tries< 5 ){ esp_wifi_connect ();num_of_tries++; ESP_LOGI("R", "Reconnecting...\n" );} 
    } 
    else  if (event_id == IP_EVENT_STA_GOT_IP)//if we catch the ip the wifi driver works correctly
    { 
        ESP_LOGI("R","Wifi got IP...\n\n" ); 
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("R", "Wifi got IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));//we got the ip and show it
        //reset the variables
        num_of_tries=0;
        wifi_conected=true;
    } 
}

//old code
esp_err_t wifi_sta_init()
{
    esp_err_t ret = nvs_flash_init();//inicializamos el nvs
    
    /*En esta condicional lo que estamos haciendo es verificar si el NVS ya esta escrito
    o si contiene datos basura que no permite que el modulo de wifi pueda leer o escribir
    si es asi simplemente borramos dichos datos y volvemos a inicializar el nvs
    ESP_ERR_NVS_NO_FREE_PAGES: significa que la partición nvs está llena o dañada
    ESP_ERR_NVS_NEW_VERSION_FOUND: significa que tenemos datos viejos*/
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());//borramos el contenido que tenga el nvs
        ESP_ERROR_CHECK(nvs_flash_init());//volvemos a inicializarlo
    }

    /*Estas lineas son dificiles de explicar pero lo que basicamente hace es inicializar el
    netif que es basicamente para inciar el modulo de red ip (dhcp,ip mascaras..)
    y enlazarlas al driver del wifi
    gracias a esto la esp32 puede tener una ip propia, ademas si se dan cuenta
    en la linea final le estamos agregrando un sta es quiere decir que vamos a instanciar
    el driver el modo sta osea como ya dije vamos a entrar a una red local y que pueda
    asignarse su ip automaticamente, tambien podriamos como ap y ethernet*/

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta(); // interfaz STA

    /*Una vez ya configurado el driver de wifi lo inicializamos*/

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /*Basicamente estas lineas estamos haciendo un callback por eso tiene como parametro
    nuestra funcion, sin necesidad de pasarle los parametros que ya establecimos

    para la primera funcion le estamos diciendo que nos muestre todos los eventos que sean
    sobre el WIFI para la segunda los eventos para IP

    Vean estas lineas de codigo como una especie de  equivalente al override de java
    */
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);//evento para wifi
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);//evento para ip

    /*lo que viene acontinuacion es bastante importante y ya es la parte final a la 
    hora de conectarse a una red (OJO recuerden que tenemos la esp en modo STA es decir
    vamos a conectarnos a una red y dicha red mediante su Access Point nos asignara una ip
    si quisieramo que nuestra esp cree una red y actue como un pequeño router tendriamos
    que modificar el codigo a modo AP)*/

    wifi_config_t wifi_configuration={//accedemos al struct del wifi configuration en modo sta
        .sta={
            .ssid="",
            .password=""
        }//ssid y password las dejaremos en blanco en la siguinete linea copiaremos la contraseña y su ssid
    };
    //copiamos el ssid y contraseña y las agregamos al struct de wifi configuration
    strcpy((char*)wifi_configuration.sta.ssid,WIFI_SSID);
    strcpy((char*)wifi_configuration.sta.password,WIFI_PASS);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));//le decimos al chip que no vamos a a ser un AP si no un STA
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration));//le pasamos las configuraciones del driver
    ESP_ERROR_CHECK(esp_wifi_start());//arracamos el wifi con las configuraciones dadas
    //ESP_ERROR_CHECK(esp_wifi_connect());//y nos conectamos a la red con las contreseñas y ssid dadas en este punto nos darian una ip

    return ESP_OK;//retornamos un Ok si todo se configuro bien
}

bool have_wifi()
{
    return wifi_conected;
}