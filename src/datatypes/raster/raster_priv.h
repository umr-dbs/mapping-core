#ifndef RASTER_RASTER_PRIV_H
#define RASTER_RASTER_PRIV_H 1

#include "datatypes/raster.h"

/**
 * Base class for n dimensional rasters
 */
template<typename T, int dimensions>
class Raster : public GenericRaster {
    public:
        /**
         * Create a raster using a `DataDescription`, a `SpatioTemporalReference` as well as its `width`, `height` and `depth`
         */
        Raster(const DataDescription &datadescription, const SpatioTemporalReference &stref, uint32_t width,
               uint32_t height, uint32_t depth);

        /**
         * Destructor for the raster
         */
        virtual ~Raster();

        /**
         * Retrieve the byte size of the raster's pixels
         */
        virtual size_t getDataSize() const { return sizeof(T) * getPixelCount(); };

        /**
         * Retrieve the byte size of
         */
        virtual int getBPP() { return sizeof(T); }

        /**
         * Change representation to CPU or GPU
         */
        virtual void setRepresentation(Representation);

        /**
         * Access the data as a const void pointer
         */
        virtual const void *getData() {
            setRepresentation(GenericRaster::Representation::CPU);
            return (void *) data;
        };

        /**
         * Access the data as a void pointer
         */
        virtual void *getDataForWriting() {
            setRepresentation(GenericRaster::Representation::CPU);
            return (void *) data;
        };

        /**
         * Retrieve the underlying OpenCL Buffer if it exists
         */
        virtual cl::Buffer *getCLBuffer() { return clbuffer; };

        /**
         * Retrieve the underlying OpenCL Info Buffer if it exists
         */
        virtual cl::Buffer *getCLInfoBuffer() { return clbuffer_info; };

        /**
         * Return the total byte size of this raster
         */
        virtual size_t get_byte_size() const {
            return GenericRaster::get_byte_size() + getDataSize() + sizeof(T *) + sizeof(void *) +
                   2 * sizeof(cl::Buffer *);
        }

    protected:
        T *data;
        void *clhostptr;
        cl::Buffer *clbuffer;
        cl::Buffer *clbuffer_info;
};

class RasterOperator;

/**
 * Class that encapsulates 2 dimensional rasters
 */
template<typename T>
class Raster2D : public Raster<T, 2> {
    public:
        /**
         * Create a raster using a `DataDescription`, a `SpatioTemporalReference` as well as its `width` and `height`
         */
        Raster2D(const DataDescription &datadescription, const SpatioTemporalReference &stref, uint32_t width,
                 uint32_t height, uint32_t depth = 0);

        /**
         * Destructor of `Raster2D`
         */
        virtual ~Raster2D();

        /**
         * Create a Netpbm grayscale image file
         */
        virtual void toPGM(const char *filename, bool avg);

        /**
        * Create a YUV video file
        */
        virtual void toYUV(const char *filename);

        /**
        * Create a Portable Network Graphics byte stream
        */
        virtual void toPNG(std::ostream &output, const Colorizer &colorizer, bool flipx = false, bool flipy = false,
                           Raster2D<uint8_t> *overlay = nullptr);

        /**
         * Create a JPEG file
         */
        virtual void toJPEG(const char *filename, const Colorizer &colorizer, bool flipx = false, bool flipy = false);

        /**
         * Create a GDAL file based on a driver
         */
        virtual void toGDAL(const char *filename, const char *driver, bool flipx = false, bool flipy = false);

        /**
         * Clears all pixels and sets a value
         */
        virtual void clear(double value);

        /**
         * Copies this raster's values to an offset of a destination raster
         */
        virtual void blit(const GenericRaster *raster, int x, int y = 0, int z = 0);

        /**
         * Modifies this raster by cutting out a rectangular portion
         */
        virtual std::unique_ptr<GenericRaster> cut(int x, int y, int z, int width, int height, int depths);

        /**
         *  Resizes this raster with scale parameters
         */
        virtual std::unique_ptr<GenericRaster> scale(int width, int height = 0, int depth = 0);

        /**
         * Flips the current raster along axes
         */
        virtual std::unique_ptr<GenericRaster> flip(bool flipx, bool flipy);

        /**
         * Modifies this raster by cutting out the rectangular region of the query rectangle `qrect`
         */
        virtual std::unique_ptr<GenericRaster> fitToQueryRectangle(const QueryRectangle &qrect);

        /**
         * Prints a series of chars to a region of this raster
         */
        virtual void print(int x, int y, double value, const char *text, int maxlen = -1);

        /**
         * Retrieves a pixel values as a double
         */
        virtual double getAsDouble(int x, int y = 0, int z = 0) const;

        /**
         * Retrieves a pixel value
         *
         * Performs no out of bounds check
         */
        T get(int x, int y) const {
            return data[(size_t) y * width + x];
        }

        /**
         * Retrieves a pixel value and returns the default value `def` if the call is out of bounds
         */
        T getSafe(int x, int y, T def = 0) const {
            if (x >= 0 && y >= 0 && (uint32_t) x < width && (uint32_t) y < height)
                return data[(size_t) y * width + x];
            return def;
        }

        /**
         * Writes a value to a pixel position
         *
         * Performs no out of bounds check
         */
        void set(int x, int y, T value) {
            data[(size_t) y * width + x] = value;
        }

        /**
         * Writes a value to a pixel position
         *
         * Performs no action if the position is out of bounds
         */
        void setSafe(int x, int y, T value) {
            if (x >= 0 && y >= 0 && (uint32_t) x < width && (uint32_t) y < height)
                data[(size_t) y * width + x] = value;
        }

        // create aliases for parent classes members
        // otherwise we'd need to write this->data every time
        // see: two-phase lookup of dependant names
    public:
        using Raster<T, 2>::stref;
        using Raster<T, 2>::width;
        using Raster<T, 2>::height;
        using Raster<T, 2>::pixel_scale_x;
        using Raster<T, 2>::pixel_scale_y;
        using Raster<T, 2>::PixelToWorldX;
        using Raster<T, 2>::PixelToWorldY;
        using Raster<T, 2>::WorldToPixelX;
        using Raster<T, 2>::WorldToPixelY;
        using Raster<T, 2>::data;
        using Raster<T, 2>::dd;
        using Raster<T, 2>::getPixelCount;
        using Raster<T, 2>::getDataSize;
        using Raster<T, 2>::setRepresentation;
    private:
        auto writePallettedPng(std::ostream &output, const Colorizer &colorizer, bool flipx, bool flipy,
                               Raster2D<uint8_t> *overlay) const -> void;

        auto writeRgbaBytesPng(std::ostream &output, bool flipx, bool flipy, const Raster2D<uint8_t> *overlay) const -> void;
};


#define RASTER_PRIV_INSTANTIATE_ALL template class Raster2D<uint8_t>;template class Raster2D<uint16_t>;template class Raster2D<int16_t>;template class Raster2D<uint32_t>;template class Raster2D<int32_t>;template class Raster2D<float>;template class Raster2D<double>;


#endif
