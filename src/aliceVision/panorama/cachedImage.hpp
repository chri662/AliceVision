#pragma once

#include <aliceVision/image/all.hpp>
#include <aliceVision/image/cache.hpp>
#include <aliceVision/panorama/boundingBox.hpp>
#include <aliceVision/system/Logger.hpp>
#include <aliceVision/types.hpp>

namespace aliceVision
{

template <class T>
class CachedImage
{
public:
    bool createImage(std::shared_ptr<image::TileCacheManager> manager, size_t width, size_t height)
    {

        _width = width;
        _height = height;
        _tileSize = manager->getTileWidth();

        _tilesArray.clear();

        int countHeight = int(ceil(double(height) / double(manager->getTileHeight())));
        int countWidth = int(ceil(double(width) / double(manager->getTileWidth())));

        _memoryWidth = countWidth * _tileSize;
        _memoryHeight = countHeight * _tileSize;

        for(int i = 0; i < countHeight; i++)
        {

            int tile_height = manager->getTileHeight();
            if(i == countHeight - 1)
            {
                tile_height = height - (i * tile_height);
            }

            std::vector<image::CachedTile::smart_pointer> row;

            for(int j = 0; j < countWidth; j++)
            {

                int tile_width = manager->getTileWidth();
                if(j == countWidth - 1)
                {
                    tile_width = width - (i * tile_width);
                }

                image::CachedTile::smart_pointer tile = manager->requireNewCachedTile<T>(tile_width, tile_height);
                if(tile == nullptr)
                {
                    return false;
                }

                row.push_back(tile);
            }

            _tilesArray.push_back(row);
        }

        return true;
    }

    bool writeImage(const std::string& path)
    {

        ALICEVISION_LOG_ERROR("incorrect template function");
        return false;
    }

    template <class UnaryFunction>
    bool perPixelOperation(UnaryFunction f)
    {

        for(int i = 0; i < _tilesArray.size(); i++)
        {

            std::vector<image::CachedTile::smart_pointer>& row = _tilesArray[i];

            for(int j = 0; j < _tilesArray[i].size(); j++)
            {

                image::CachedTile::smart_pointer ptr = row[j];
                if(!ptr)
                {
                    continue;
                }

                if(!ptr->acquire())
                {
                    continue;
                }

                T* data = (T*)ptr->getDataPointer();

                std::transform(data, data + ptr->getTileWidth() * ptr->getTileHeight(), data, f);
            }
        }

        return true;
    }

    template <class T2, class BinaryFunction>
    bool perPixelOperation(CachedImage<T2> & other,  BinaryFunction f)
    {
        if (other.getWidth() != _width || other.getHeight() != _height) 
        {
            return false;
        }

        for(int i = 0; i < _tilesArray.size(); i++)
        {
            std::vector<image::CachedTile::smart_pointer>& row = _tilesArray[i];
            std::vector<image::CachedTile::smart_pointer>& rowOther = other.getTiles()[i];

            for(int j = 0; j < _tilesArray[i].size(); j++)
            {

                image::CachedTile::smart_pointer ptr = row[j];
                if(!ptr)
                {
                    continue;
                }

                image::CachedTile::smart_pointer ptrOther = rowOther[j];
                if(!ptrOther)
                {
                    continue;
                }

                if(!ptr->acquire())
                {
                    continue;
                }

                if(!ptrOther->acquire())
                {
                    continue;
                }

                T* data = (T*)ptr->getDataPointer();
                T2* dataOther = (T2*)ptrOther->getDataPointer();

                std::transform(data, data + ptr->getTileWidth() * ptr->getTileHeight(), dataOther, data, f);
            }
        }

        return true;
    }

    bool deepCopy(CachedImage<T> & source)
    {
        if (source._memoryWidth != _memoryWidth) return false;
        if (source._memoryHeight != _memoryHeight) return false;
        if (source._tileSize != _tileSize) return false;

        for(int i = 0; i < _tilesArray.size(); i++)
        {
            std::vector<image::CachedTile::smart_pointer> & row = _tilesArray[i];
            std::vector<image::CachedTile::smart_pointer> & rowSource = source._tilesArray[i];

            for(int j = 0; j < _tilesArray[i].size(); j++)
            {

                image::CachedTile::smart_pointer ptr = row[j];
                if(!ptr)
                {
                    continue;
                }

                image::CachedTile::smart_pointer ptrSource = rowSource[j];
                if(!ptrSource)
                {
                    continue;
                }

                if (!ptr->acquire())
                {
                    continue;
                }

                if (!ptrSource->acquire())
                {
                    continue;
                }

                T * data = (T*)ptr->getDataPointer();
                T * dataSource = (T*)ptrSource->getDataPointer();
                
                std::memcpy(data, dataSource, _tileSize * _tileSize * sizeof(T));
            }   
        }

        return true;
    }

    bool assign(const aliceVision::image::Image<T>& input, const BoundingBox & inputBb, const BoundingBox & outputBb)
    {
        BoundingBox outputMemoryBb;
        outputMemoryBb.left = 0;
        outputMemoryBb.top = 0;
        outputMemoryBb.width = _width;
        outputMemoryBb.height = _height;

        if(!outputBb.isInside(outputMemoryBb))
        {
            return false;
        }

        BoundingBox inputMemoryBb;
        inputMemoryBb.left = 0;
        inputMemoryBb.top = 0;
        inputMemoryBb.width = input.Width();
        inputMemoryBb.height = input.Height();

        if(!inputBb.isInside(inputMemoryBb))
        {
            return false;
        }

        if (inputBb.width != outputBb.width) 
        {
            return false;
        }

        if (inputBb.height != outputBb.height) 
        {
            return false;
        }


        //Make sure we have our bounding box aligned with the tiles
        BoundingBox snapedBb = outputBb;
        snapedBb.snapToGrid(_tileSize);

        //Compute grid parameters
        BoundingBox gridBb;
        gridBb.left = snapedBb.left / _tileSize;
        gridBb.top = snapedBb.top / _tileSize;
        gridBb.width = snapedBb.width / _tileSize;
        gridBb.height = snapedBb.height / _tileSize;

        int delta_y = outputBb.top - snapedBb.top;
        int delta_x = outputBb.left - snapedBb.left;

        for(int i = 0; i < gridBb.height; i++)
        {
            //ibb.top + i * tileSize --> snapedBb.top + delta + i * tileSize
            int ti = gridBb.top + i;
            int sy = inputBb.top - delta_y + i * _tileSize;
            
            std::vector<image::CachedTile::smart_pointer>& row = _tilesArray[ti];

            for(int j = 0; j < gridBb.width; j++)
            {
                int tj = gridBb.left + j;
                int sx = inputBb.left - delta_x + j * _tileSize;

                image::CachedTile::smart_pointer ptr = row[tj];
                if(!ptr)
                {
                    continue;
                }

                if(!ptr->acquire())
                {
                    continue;
                }

                T* data = (T*)ptr->getDataPointer();

                for(int y = 0; y < _tileSize; y++)
                {
                    for(int x = 0; x < _tileSize; x++)
                    {
                        if (sy + y < 0 || sy + y >= input.Height()) continue;
                        if (sx + x < 0 || sx + x >= input.Width()) continue;
                        data[y * _tileSize + x] = input(sy + y, sx + x);
                    }
                }
            }
        }

        return true;
    }

    bool extract(aliceVision::image::Image<T>& output, const BoundingBox & outputBb, const BoundingBox& inputBb)
    {
        BoundingBox thisBb;
        thisBb.left = 0;
        thisBb.top = 0;
        thisBb.width = _width;
        thisBb.height = _height;

        if(!inputBb.isInside(thisBb))
        {
            return false;
        }

        BoundingBox outputMemoryBb;
        outputMemoryBb.left = 0;
        outputMemoryBb.top = 0;
        outputMemoryBb.width = output.Width();
        outputMemoryBb.height = output.Height();

        if(!outputBb.isInside(outputMemoryBb))
        {
            return false;
        }

        if (inputBb.width != outputBb.width) 
        {
            return false;
        }

        if (inputBb.height != outputBb.height) 
        {
            return false;
        }


        BoundingBox snapedBb = inputBb;
        snapedBb.snapToGrid(_tileSize);

        BoundingBox gridBb;
        gridBb.left = snapedBb.left / _tileSize;
        gridBb.top = snapedBb.top / _tileSize;
        gridBb.width = snapedBb.width / _tileSize;
        gridBb.height = snapedBb.height / _tileSize;

        int delta_y = inputBb.top - snapedBb.top;
        int delta_x = inputBb.left - snapedBb.left;

        for(int i = 0; i < gridBb.height; i++)
        {
            int ti = gridBb.top + i;
            int sy = outputBb.top - delta_y + i * _tileSize;


            std::vector<image::CachedTile::smart_pointer>& row = _tilesArray[ti];

            for(int j = 0; j < gridBb.width; j++)
            {
                int tj = gridBb.left + j;
                int sx = outputBb.left - delta_x + j * _tileSize;

                image::CachedTile::smart_pointer ptr = row[tj];
                if(!ptr)
                {
                    continue;
                }

                if(!ptr->acquire())
                {
                    continue;
                }

                T* data = (T*)ptr->getDataPointer();

                for(int y = 0; y < _tileSize; y++)
                {
                    for(int x = 0; x < _tileSize; x++)
                    {
                        if (sy + y < 0 || sy + y >= output.Height()) continue;
                        if (sx + x < 0 || sx + x >= output.Width()) continue;
                        if (y < 0 || x < 0) continue;

                        output(sy + y, sx + x) = data[y * _tileSize + x];
                    }
                }
            }
        }

        return true;
    }

    std::vector<std::vector<image::CachedTile::smart_pointer>>& getTiles() { return _tilesArray; }

    int getWidth() const { return _width; }

    int getHeight() const { return _height; }

private:
    int _width;
    int _height;
    int _memoryWidth;
    int _memoryHeight;
    int _tileSize;

    std::vector<std::vector<image::CachedTile::smart_pointer>> _tilesArray;
};

template <>
bool CachedImage<image::RGBAfColor>::writeImage(const std::string& path);

template <>
bool CachedImage<image::RGBfColor>::writeImage(const std::string& path);

template <>
bool CachedImage<IndexT>::writeImage(const std::string& path);

} // namespace aliceVision