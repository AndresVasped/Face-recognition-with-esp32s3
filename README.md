## 1. Integration 
Esta guía resume los pasos para integrar un sistema de reconocimiento facial Edge AI en un ESP32-S3 utilizando la librería ESP-DL v3.3.3, evitando los errores comunes de configuración y memoria que pueden bloquear tu proyecto.

## 2. vision of the project
Este proyecto consiste en el diseño e implementación de un sistema embebido de seguridad y visión artificial en tiempo real utilizando el microcontrolador ESP32-S3. El núcleo del sistema es la ejecución local de un modelo de Deep Learning para la detección y el reconocimiento facial, optimizando los recursos de hardware limitados del chip para tomar decisiones de forma autónoma (en el Edge) y reportar eventos críticos a través de una plataforma externa (Telegram).

## 3. Components 
* Hardware: ESP32-S3 con cámara (ej: OV3660)
* Framework: ESP-IDF v5.5.0
* Librería IA: ESP-DL v3.3.3 (Human Face Recognition)
* Particiones Flash: Personalizadas según tamaño de modelos
* Empaquetado Manual: Script Python para preparar modelos

## 4. Configuration step by step
**i) Choose the right model**

No todos los modelos son iguales. Elige según tu necesidad:
| Detector | Landmarks | Velocidad | Uso |
| :--- | :--- | :--- | :--- |
| `ESPDET_PICO` | ❌ | Alta | Detección rápida sin reconocimiento |
| `MSRMNP_S8_V1` | ✅ | Media | Detección + landmarks (requerido para reconocimiento) |

Para reconocimiento facial, **debes usar** MSRMNP_S8_V1.

---

**ii) partitions table (partitions.csv)**

Asegúrate de que tus particiones tengan el tamaño adecuado. Los modelos deben caber completamente.

```csv
# Nombre      Tipo      Subtipo   Offset      Size
nvs            data      nvs       0x9000      0x5000
otadata        data      ota       0xe000      0x2000
app0           app       ota_0     0x10000     0x800000
spiffs         data      spiffs    0x810000    0x300000
human_face_det data      0x40      0xB10000    0x100000  # 1MB para MSR + MNP
human_face_feat data      0x41      0xC10000    0x200000  # 2MB para MFN
```

>  **Important:** Verifica el tamaño real de tus modelos con PowerShell:
>
> ```powershell
> Get-Item "route\model.espdl" | Select-Object Name, Length
> ```

---

**iii) Manual Packaging of Models**

El sistema `FbsLoader` requiere que los modelos estén empaquetados con una cabecera especial. Usa el script oficial:

```powershell
# Empaquetar detector (MSR + MNP)
python components/esp-dl/fbs_loader/pack_espdl_models.py \
  --model_path \
    components/esp-dl/models/human_face_detect/models/s3/human_face_detect_msr_s8_v1.espdl \
    components/esp-dl/models/human_face_detect/models/s3/human_face_detect_mnp_s8_v1.espdl \
  --out_file human_face_detect_packed.espdl

# Empaquetar reconocedor (MFN)
python components/esp-dl/fbs_loader/pack_espdl_models.py \
  --model_path components/esp-dl/models/human_face_recognition/models/s3/human_face_feat_mfn_s8_v1.espdl \
  --out_file human_face_feat_packed.espdl
```

---

**iv) Manual flashing of models**

Flashea los binarios empaquetados a las particiones correspondientes:

```powershell
esptool.py --chip esp32s3 write_flash 0xB10000 human_face_detect_packed.espdl
esptool.py --chip esp32s3 write_flash 0xC10000 human_face_feat_packed.espdl
```

---

**v) right init code**

Instancia los objetos de alto nivel, no las clases base:

```cpp
#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"

dl::detect::Detect *face_detector = nullptr;
dl::recognition::Recognizer *face_recognizer = nullptr;

extern "C" void models_init()
{
    // Detect with landmarks
    face_detector = new HumanFaceDetect(HumanFaceDetect::MSRMNP_S8_V1, false);

    // db
    face_recognizer = new HumanFaceRecognizer("/spiffs/face.db", HumanFaceFeat::MFN_S8_V1, false);
}
```

---

## 3. Common errors and how to solve it 

| Error | Causa | Solución |
|-------|-------|----------|
| `EXCVADDR: 0x00000098` | Clases base sin modelo | Usar `HumanFaceDetect`/`Recognizer` |
| `Cache/MMU entry fault` | Partición muy pequeña | Aumentar tamaño en `partitions.csv` |
| `landmarks.size() == 10` | Detector sin landmarks | Usar `MSRMNP_S8_V1` |
| `Unsupported format` | Modelo sin empaquetar | Usar `pack_espdl_models.py` |

---

## Recomendations to debug

1. **Verifica siempre el tamaño de los modelos** antes de ajustar particiones.
2. **Usa el monitor serial para ver mensajes de error específicos.**
3. Si hay dudas, borra el archivo `sdkconfig` y haz `Clean + Build` para regenerar configuraciones limpias.

---

## 4. restrictions
  1. Tamaño de particiones: los modelos empaquetados deben caber exactamente en la partición definida en partitions.csv. Si el modelo es más grande que la partición, el MMU intentará mapear direcciones fuera de rango → Cache/MMU faults y reboot.
  2. Empaquetado de modelos: no flashees los .espdl crudos si el componente espera un archivo “packed” con cabecera; usa pack_espdl_models.py para producir el .espdl final que FbsLoader entiende.
  3. Alineación DMA / PSRAM: los buffers usados por la cámara (especialmente en modo PSRAM DMA) deben cumplir requisitos de alineación (16/32 bytes típicamente). Si el modelo o el preprocesador asume alineación y no la encuentra, pueden producirse fallos o lecturas corruptas. Usa heap_caps_aligned_alloc para buffers intermedios si hace falta.
  4. Formato de imagen: muchas internals de esp-dl esperan RGB888. Si la cámara entrega RGB565, conviértelo manualmente (fmt2rgb888) antes de construir img_t para evitar conversiones internas problemáticas.
  5. Dimensiones: modelos NN suelen requerir ancho/alto múltiplos de 16 — usa resoluciones como 320x240 o 416x416 según el detector.
  6. Inicialización de objetos: usa las clases de alto nivel (HumanFaceDetect, HumanFaceRecognizer) que cargan modelos y configuran internals; instanciar implementaciones base sin modelos puede dejar punteros/vtables en NULL.
  7. sdkconfig vs build_flags: CMake (Kconfig) necesita sdkconfig/sdconfig.defaults para empaquetar y flashear; el compilador C++ necesita -D flags para que los headers vean CONFIG_* — ambos deben estar alineados.
  8. Offsets y flasheo: cuando flashees manualmente, asegúrate de usar los offsets exactos (ej. 0xB10000 para detector empaquetado, 0xB90000 para feat empaquetado) y no mapear la misma región dos veces.
  
    
## Conclusion
Integrar IA en microcontroladores es potente pero requiere precisión en configuraciones de bajo nivel. Siguiendo estos pasos, puedes evitar horas de frustración y centrarte en desarrollar aplicaciones reales con visión artificial en el borde. La mayoria de ibrerias de IA estan escritas en C++ por lo que si tu trabajas netamente en C ten en cuenta eso y adapta tu proyecto a una arquitectura hibrida entre C y C++.
Para finalizar integrar IA en un microcontrolador es muy satisfactorio asi que no te desanimes y animo
