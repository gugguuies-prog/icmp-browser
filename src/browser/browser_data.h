/**
 * browser_data.h
 * 
 * Browser data management: history, bookmarks, cache
 */

#ifndef BROWSER_DATA_H
#define BROWSER_DATA_H

#include <string>
#include <vector>
#include <map>
#include <ctime>

/**
 * History entry
 */
struct HistoryEntry {
    std::string url;
    std::string title;
    time_t timestamp;
    int visitCount;
};

/**
 * Bookmark entry
 */
struct Bookmark {
    std::string url;
    std::string title;
    time_t created;
};

/**
 * Cache entry
 */
struct CacheEntry {
    std::string url;
    std::string content;
    time_t timestamp;
    size_t size;
};

/**
 * Browser data manager
 */
class BrowserData {
public:
    BrowserData(const std::string& dataDir);
    ~BrowserData();
    
    // History management
    void AddHistory(const std::string& url, const std::string& title);
    std::vector<HistoryEntry> GetHistory(int limit = 100);
    void ClearHistory();
    
    // Bookmark management
    void AddBookmark(const std::string& url, const std::string& title);
    void RemoveBookmark(const std::string& url);
    std::vector<Bookmark> GetBookmarks();
    bool IsBookmarked(const std::string& url);
    
    // Cache management
    void CacheContent(const std::string& url, const std::string& content);
    bool GetCachedContent(const std::string& url, std::string& content);
    void ClearCache();
    size_t GetCacheSize();
    
    // Persistence
    bool Load();
    bool Save();
    
private:
    std::string m_dataDir;
    std::vector<HistoryEntry> m_history;
    std::vector<Bookmark> m_bookmarks;
    std::map<std::string, CacheEntry> m_cache;
    
    std::string GetHistoryFile();
    std::string GetBookmarksFile();
    std::string GetCacheFile();
    
    bool LoadHistory();
    bool LoadBookmarks();
    bool LoadCache();
    
    bool SaveHistory();
    bool SaveBookmarks();
    bool SaveCache();
};

#endif // BROWSER_DATA_H
