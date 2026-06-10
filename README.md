## 1. Integration Overview
This guide summarizes the steps to integrate an Edge AI face recognition system into an ESP32-S3 using the ESP-DL v3.3.3 library, avoiding common memory and configuration errors that can easily block your project.

## 2. Project Vision
This project consists of the design and implementation of a real-time embedded security and computer vision system using the ESP32-S3 microcontroller. The core of the system is the local execution of a Deep Learning model for facial detection and recognition, optimizing the limited hardware resources of the chip to make autonomous decisions at the Edge and report critical events via an external platform (Telegram).

## 3. Components 
* **Hardware:** ESP32-S3 with camera module (e.g., OV2640 / OV3660)
* **Framework:** ESP-IDF v5.5.0
* **AI Library:** ESP-DL v3.3.3 (Human Face Recognition)
* **Flash Partitions:** Custom-sized according to the models' footprint
* **Manual Packaging:** Python script to prepare and pack the models

---

## 4. Hardware Configuration (Pinout)
This project configures the OV2640 camera module with the following pin mapping. Make sure your wiring matches this setup on your ESP32-S3:

```c
#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5

#define CAM_PIN_D7 16
#define CAM_PIN_D6 17
#define CAM_PIN_D5 18
#define CAM_PIN_D4 12
#define CAM_PIN_D3 10
#define CAM_PIN_D2 8
#define CAM_PIN_D1 9
#define CAM_PIN_D0 11

#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13
```
---

## 5. Step-by-Step Configuration

**i) Choose the right model**

Not all models are created equal. Choose according to your needs:
| Detector | Landmarks | Speed | Use Case |
| :--- | :--- | :--- | :--- |
| `ESPDET_PICO` | ❌ | High | Fast detection without recognition |
| `MSRMNP_S8_V1` | ✅ | Medium | Detection + landmarks (required for recognition) |

For face recognition, **you must use** `MSRMNP_S8_V1`.

**ii) Partitions table (partitions.csv)**

Ensure your partitions have the correct size. The models must fit entirely within their allocated space.

```csv
# Name      Type      SubType   Offset      Size
nvs            data      nvs       0x9000      0x5000
otadata        data      ota       0xe000      0x2000
app0           app       ota_0     0x10000     0x800000
spiffs         data      spiffs    0x810000    0x300000
human_face_det data      0x40      0xB10000    0x100000  # 1MB for MSR + MNP
human_face_feat data      0x41      0xC10000    0x200000  # 2MB for MFN
```

>  **Important:** Verify the actual size of your models using PowerShell:
>
> ```powershell
> Get-Item "route\model.espdl" | Select-Object Name, Length
> ```

**iii) Manual Packaging of Models**

The `FbsLoader` system requires models to be packaged with a special header. Use the official script:

```powershell
# Pack detector (MSR + MNP)
python components/esp-dl/fbs_loader/pack_espdl_models.py \
  --model_path \
    components/esp-dl/models/human_face_detect/models/s3/human_face_detect_msr_s8_v1.espdl \
    components/esp-dl/models/human_face_detect/models/s3/human_face_detect_mnp_s8_v1.espdl \
  --out_file human_face_detect_packed.espdl

# Pack recognizer (MFN)
python components/esp-dl/fbs_loader/pack_espdl_models.py \
  --model_path components/esp-dl/models/human_face_recognition/models/s3/human_face_feat_mfn_s8_v1.espdl \
  --out_file human_face_feat_packed.espdl
```

**iv) Manual flashing of models**

Flash the packed binaries to the corresponding partitions using `esptool.py`:

```powershell
esptool.py --chip esp32s3 write_flash 0xB10000 human_face_detect_packed.espdl
esptool.py --chip esp32s3 write_flash 0xC10000 human_face_feat_packed.espdl
```

**v) Right initialization code**

Instantiate the high-level objects, not the base classes:

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

## 6. Demonstration / Results
Since the inference is done completely on the Edge (inside the ESP32-S3) without a web interface, the results are output directly via the Serial Monitor. 

Below is the real-time execution log demonstrating the successful face detection, the confidence score of the recognition, and the HTTP POST request confirming the action:

<img width="618" height="66" alt="image" src="https://github.com/user-attachments/assets/15986dd3-067f-4c2f-9b18-1f2182addc8e" />

---

## 7. Common Errors and Solutions

| Error | Cause | Solution |
|-------|-------|----------|
| `EXCVADDR: 0x00000098` | Base classes initialized without model | Use `HumanFaceDetect` / `Recognizer` |
| `Cache/MMU entry fault` | Partition is too small | Increase size in `partitions.csv` |
| `landmarks.size() == 10` | Detector missing landmarks | Use `MSRMNP_S8_V1` |
| `Unsupported format` | Flashed unpacked (raw) model | Use `pack_espdl_models.py` |

---

## 8. Debugging Recommendations

1. **Always verify the model sizes** before adjusting your partition tables.
2. **Use the Serial Monitor** to check for specific ESP-IDF boot or kernel panic error messages.
3. When in doubt, delete the `sdkconfig` file and run a `Clean + Build` to generate fresh configurations.

---

## 9. Technical Constraints & Considerations
  1. **Partition size:** The packed models must fit exactly within the partition defined in `partitions.csv`. If the model is larger than the partition, the MMU will attempt to map out-of-bounds addresses, leading to Cache/MMU faults and a reboot.
  2. **Model packaging:** Do not flash raw `.espdl` files if the component expects a "packed" file with a header. Always use `pack_espdl_models.py` to produce the final `.espdl` that `FbsLoader` understands.
  3. **DMA / PSRAM Alignment:** Buffers used by the camera (especially in PSRAM DMA mode) must meet strict alignment requirements (typically 16/32 bytes). If the model or preprocessor assumes alignment and doesn't find it, crashes or corrupt memory reads will occur. Use `heap_caps_aligned_alloc` for intermediate I/O buffers if necessary.
  4. **Image format:** Many ESP-DL internals expect `RGB888`. If your camera outputs `RGB565`, convert it manually (e.g., using `fmt2rgb888`) before building the `img_t` structure to avoid problematic internal conversions.
  5. **Dimensions:** Neural Network models usually require width/height to be multiples of 16. Ensure you use supported resolutions like 320x240 or 416x416 depending on the detector requirements.
  6. **Object initialization:** Always use the provided high-level classes (`HumanFaceDetect`, `HumanFaceRecognizer`) which handle model loading and internal setups correctly. Instantiating base implementations without models can leave pointers/vtables at `NULL`.
  7. **sdkconfig vs build_flags:** CMake (Kconfig) relies on `sdkconfig`/`sdkconfig.defaults` to package and flash correctly. Meanwhile, the C++ compiler needs `-D` flags so the headers can see `CONFIG_*`. Make sure both are aligned.
  8. **Offsets and flashing:** When flashing manually, ensure you use the exact target offsets (e.g., `0xB10000` for the packed detector, `0xC10000` for the packed recognizer) and avoid mapping overlapping flash regions.

---
  
    
## 10. Conclusion
Integrating Artificial Intelligence into microcontrollers is incredibly powerful, but it requires precision in low-level configuration. By following these steps and constraints, you can prevent hours of frustration and focus on developing real-world computer vision applications at the Edge. 

*Note:* The vast majority of Edge AI libraries are written in C++. If your main codebase is strictly `C`, keep this in mind and structure your project around a hybrid `C/C++` architecture to interface with the libraries smoothly.

Finally, integrating AI into a resource-constrained microcontroller is highly rewarding. Don't get discouraged when facing Kernal Panics, and keep pushing forward!
```
