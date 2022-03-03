#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include "fcyIO/fcyStream.h"

namespace LuaSTG::Core
{
    enum class FileType
    {
        Unknown = 0,
        File = 0x1,
        Directory = 0x2,
    };
    
    struct FileNode
    {
        FileType type;
        std::string name;
    };
    
    class FileNodeTree
    {
    public:
        virtual size_t findIndex(std::string_view const& name) = 0;
        virtual size_t getCount() = 0;
        virtual FileType getType(size_t index) = 0;
        virtual std::string_view getName(size_t index) = 0;
        virtual bool contain(std::string_view const& name) = 0;
        virtual bool load(std::string_view const& name, std::vector<uint8_t>& buffer) = 0;
        virtual bool load(std::string_view const& name, fcyMemStream** buffer) = 0;
    };
    
    class FileArchive : public FileNodeTree
    {
    private:
        std::string name;
        uint64_t uuid = 0;
        void* zip_v = nullptr;
    public:
        size_t findIndex(std::string_view const& name);
        size_t getCount();
        FileType getType(size_t index);
        std::string_view getName(size_t index);
        bool contain(std::string_view const& name);
        bool load(std::string_view const& name, std::vector<uint8_t>& buffer);
        bool load(std::string_view const& name, fcyMemStream** buffer);
    public:
        bool empty();
        uint64_t getUUID();
        std::string_view getFileArchiveName();
        bool setPassword(std::string_view const& password);
        bool loadEncrypted(std::string_view const& name, std::string_view const& password, std::vector<uint8_t>& buffer);
        bool loadEncrypted(std::string_view const& name, std::string_view const& password, fcyMemStream** buffer);
    public:
        FileArchive() = default;
        FileArchive(std::string_view const& path);
        ~FileArchive();
    };
    
    class FileManager : public FileNodeTree
    {
    private:
        std::vector<FileNode> list;
        std::vector<std::string> search_list;
        FileArchive null_archive;
        std::vector<std::shared_ptr<FileArchive>> archive;
        void refresh();
    public:
        size_t findIndex(std::string_view const& name);
        size_t getCount();
        FileType getType(size_t index);
        std::string_view getName(size_t index);
        bool contain(std::string_view const& name);
        bool load(std::string_view const& name, std::vector<uint8_t>& buffer);
        bool load(std::string_view const& name, fcyMemStream** buffer);
    public:
        size_t getFileArchiveCount();
        FileArchive& getFileArchiveByUUID(uint64_t uuid);
        FileArchive& getFileArchive(size_t index);
        FileArchive& getFileArchive(std::string_view const& name);
        bool loadFileArchive(std::string_view const& name);
        bool loadFileArchive(std::string_view const& name, std::string_view const& password);
        bool containFileArchive(std::string_view const& name);
        void unloadFileArchive(std::string_view const& name);
        void unloadAllFileArchive();
    public:
        void addSearchPath(std::string_view const& path);
        void removeSearchPath(std::string_view const& path);
        void clearSearchPath();
    public:
        bool containEx(std::string_view const& name);
        bool loadEx(std::string_view const& name, std::vector<uint8_t>& buffer);
        bool loadEx(std::string_view const& name, fcyMemStream** buffer);
    public:
        FileManager();
        ~FileManager();
    public:
        static FileManager& get();
    };
}

inline LuaSTG::Core::FileManager& GFileManager()
{
    return LuaSTG::Core::FileManager::get();
}