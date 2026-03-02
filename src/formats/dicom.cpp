// SPDX-License-Identifier: MIT
// DICOM format decoder.
// Copyright (C) 2024 Artem Senichev <artemsen@gmail.com>

#include "../imageloader.hpp"

#include <cstring>
#include <limits>

// register format in factory
class ImageDicom;
static const ImageLoader::Registrator<ImageDicom>
    image_format_registartion("DICOM", ImageLoader::Priority::Low);

/** Data input stream. */
struct DataStream {
    DataStream(const std::vector<uint8_t>& raw_data, size_t pos)
        : data(raw_data)
        , position(pos)
    {
    }

    /**
     * Read data from the stream.
     * @param bytes number of bytes to consume
     * @return pointer to the data or nullptr on End of stream
     */
    const uint8_t* consume(const size_t bytes)
    {
        const uint8_t* ptr = nullptr;
        const size_t end = position + bytes;

        if (end <= data.size()) {
            ptr = data.data() + position;
            position = end;
        }

        return ptr;
    }

    /**
     * Read data from the stream.
     * @param value buffer for data to read
     * @return false on End of stream
     */
    template <typename T> bool read(T& value)
    {
        const uint8_t* ptr = consume(sizeof(T));
        if (ptr) {
            value = *reinterpret_cast<const T*>(ptr);
        }
        return ptr;
    }

private:
    const std::vector<uint8_t>& data;
    uint64_t position;
};

/* DICOM image. */
class ImageDicom : public Image {
private:
    // DICOM signature
    static constexpr const uint8_t signature[] = { 'D', 'I', 'C', 'M' };
    static constexpr const size_t DICOM_SIGNATURE_OFFSET = 128;

    // DICOM tags
    static constexpr const uint32_t TAG_SAMPLES_PER_PIXEL = 0x00280002;
    static constexpr const uint32_t TAG_ROWS = 0x00280010;
    static constexpr const uint32_t TAG_COLUMNS = 0x00280011;
    static constexpr const uint32_t TAG_BIT_ALLOCATED = 0x00280100;
    static constexpr const uint32_t TAG_SMALL_PIXEL_VAL = 0x00280106;
    static constexpr const uint32_t TAG_BIG_PIXEL_VAL = 0x00280107;
    static constexpr const uint32_t TAG_PIXEL_DATA = 0x7fe00010;

    // DICOM element value types
    enum ValueRepresentation : uint16_t {
        VR_AE = 'A' | ('E' << 8),
        VR_AS = 'A' | ('S' << 8),
        VR_AT = 'A' | ('T' << 8),
        VR_CS = 'C' | ('S' << 8),
        VR_DA = 'D' | ('A' << 8),
        VR_DS = 'D' | ('S' << 8),
        VR_DT = 'D' | ('T' << 8),
        VR_FD = 'F' | ('D' << 8),
        VR_FL = 'F' | ('L' << 8),
        VR_IS = 'I' | ('S' << 8),
        VR_LO = 'L' | ('O' << 8),
        VR_LT = 'L' | ('T' << 8),
        VR_PN = 'P' | ('N' << 8),
        VR_SH = 'S' | ('H' << 8),
        VR_SL = 'S' | ('L' << 8),
        VR_SS = 'S' | ('S' << 8),
        VR_ST = 'S' | ('T' << 8),
        VR_TM = 'T' | ('M' << 8),
        VR_UI = 'U' | ('I' << 8),
        VR_UL = 'U' | ('L' << 8),
        VR_US = 'U' | ('S' << 8),
        VR_UT = 'U' | ('T' << 8),
        VR_OB = 'O' | ('B' << 8),
        VR_OW = 'O' | ('W' << 8),
        VR_SQ = 'S' | ('Q' << 8),
        VR_UN = 'U' | ('N' << 8),
        VR_QQ = 'Q' | ('Q' << 8),
        VR_RT = 'R' | ('T' << 8),
    };

    // DICOM image description
    struct DicomImage {
        uint16_t spp;        ///< Samples per Pixel
        uint16_t bpp;        ///< Number of bits allocated for each pixel sample
        uint16_t width;      ///< Image width
        uint16_t height;     ///< Image height
        int16_t px_min;      ///< Min pixel value encountered in the image
        int16_t px_max;      ///< Max pixel value encountered in the image
        const uint8_t* data; ///< Image data
        size_t data_sz;      ///< Size of data in bytes
    };

    // DICOM element description
    struct Element {
        uint32_t tag;
        uint16_t vr;
        const uint8_t* data;
        uint32_t size;
    };

    /**
     * Read next data element from the stream.
     * @param stream binary stream
     * @param element output element description
     * @return false if no more elements int the stream
     */
    bool next_element(DataStream& stream, Element& element) const
    {
        // read tag
        if (!stream.read(element.tag)) {
            return false;
        }
        element.tag = (element.tag << 16) | (element.tag >> 16);

        // read value representation (type)
        if (!stream.read(element.vr)) {
            return false;
        }

        // get payload size
        uint16_t size;
        if (!stream.read(size)) {
            return false;
        }
        element.size = size;
        if (element.size == 0 &&
            (element.vr == VR_OB || element.vr == VR_OW ||
             element.vr == VR_SQ || element.vr == VR_UN ||
             element.vr == VR_UT)) {
            if (!stream.read(element.size)) {
                return false;
            }
        }

        // get payload data
        if (element.size == 0) {
            element.data = nullptr;
        } else if (!(element.data = stream.consume(element.size))) {
            return false;
        }

        return true;
    }

    /**
     * Get image description from the stream.
     * @param stream binary stream
     * @param image output image description
     * @return true if image description is valid
     */
    bool get_image(DataStream& stream, DicomImage& image) const
    {
        Element el;

        while (next_element(stream, el)) {
            if (!el.data) {
                continue;
            }
            if (el.tag == TAG_SAMPLES_PER_PIXEL && el.vr == VR_US) {
                image.spp = *reinterpret_cast<const uint16_t*>(el.data);
            } else if (el.tag == TAG_ROWS && el.vr == VR_US) {
                image.height = *reinterpret_cast<const uint16_t*>(el.data);
            } else if (el.tag == TAG_COLUMNS && el.vr == VR_US) {
                image.width = *reinterpret_cast<const uint16_t*>(el.data);
            } else if (el.tag == TAG_BIT_ALLOCATED && el.vr == VR_US) {
                image.bpp = *reinterpret_cast<const uint16_t*>(el.data);
            } else if (el.tag == TAG_SMALL_PIXEL_VAL && el.vr == VR_SS) {
                image.px_min = *reinterpret_cast<const uint16_t*>(el.data);
            } else if (el.tag == TAG_BIG_PIXEL_VAL && el.vr == VR_SS) {
                image.px_max = *reinterpret_cast<const uint16_t*>(el.data);
            } else if (el.tag == TAG_PIXEL_DATA && el.vr == VR_OW) {
                image.data = el.data;
                image.data_sz = el.size;
            }
        }

        return image.data && image.height && image.width &&
            image.data_sz == image.width * image.height * (image.bpp / 8);
    }

public:
    bool load(const std::vector<uint8_t>& data) override
    {
        // check signature
        if (data.size() < DICOM_SIGNATURE_OFFSET + sizeof(signature) ||
            std::memcmp(data.data() + DICOM_SIGNATURE_OFFSET, signature,
                        sizeof(signature))) {
            return false;
        }

        DataStream stream(data, DICOM_SIGNATURE_OFFSET + sizeof(signature));

        // get image description
        DicomImage dicom {};
        if (!get_image(stream, dicom) || dicom.spp != 1 /* monochrome */ ||
            dicom.bpp != 16 /* 2 bytes per pixel */) {
            return false;
        }

        // calculate min/max color value if not set yet
        if (dicom.px_max == 0 || dicom.px_max <= dicom.px_min) {
            dicom.px_min = std::numeric_limits<int16_t>::max();
            for (size_t i = 0; i < dicom.data_sz; i += sizeof(int16_t)) {
                const int16_t color =
                    *reinterpret_cast<const int16_t*>(dicom.data + i);
                if (dicom.px_max < color) {
                    dicom.px_max = color;
                }
                if (dicom.px_min > color) {
                    dicom.px_min = color;
                }
            }
        }

        // allocate pixmap
        frames.resize(1);
        Pixmap& pm = frames[0].pm;
        pm.create(Pixmap::RGB, dicom.width, dicom.height);

        // calculate coefficient for converting 16-bit color to 8-bit
        double pixel_coeff;
        if (dicom.px_max <= dicom.px_min) {
            pixel_coeff = 1.0;
        } else {
            pixel_coeff = 256.0 / (dicom.px_max - dicom.px_min);
        }

        // decode image
        const int16_t* ptr = reinterpret_cast<const int16_t*>(dicom.data);
        pm.foreach([&ptr, dicom, pixel_coeff](argb_t& pixel) {
            int16_t color = *ptr++;
            color -= dicom.px_min;
            color *= pixel_coeff;
            pixel = { argb_t::max, static_cast<uint8_t>(color),
                      static_cast<uint8_t>(color),
                      static_cast<uint8_t>(color) };
        });

        format = "DICOM";

        return true;
    }
};
