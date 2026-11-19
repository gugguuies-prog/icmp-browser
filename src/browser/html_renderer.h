/**
 * html_renderer.h
 * 
 * Simple HTML renderer for ICMP Browser
 * Converts HTML to formatted text with basic styling
 */

#ifndef HTML_RENDERER_H
#define HTML_RENDERER_H

#include <windows.h>
#include <string>
#include <vector>

/**
 * HTML Element types
 */
enum class HtmlElementType {
    TEXT,
    HEADING1,
    HEADING2,
    HEADING3,
    PARAGRAPH,
    LINK,
    IMAGE,
    LIST_ITEM,
    BOLD,
    ITALIC,
    CODE,
    PREFORMATTED
};

/**
 * Rendered HTML element with formatting
 */
struct RenderedElement {
    HtmlElementType type;
    std::string text;
    std::string url;  // For links
    COLORREF color;
    bool bold;
    bool italic;
    int fontSize;
};

/**
 * HTML Renderer class
 */
class HtmlRenderer {
public:
    HtmlRenderer();
    ~HtmlRenderer();
    
    /**
     * Parse and render HTML to formatted elements
     */
    std::vector<RenderedElement> Render(const std::string& html);
    
    /**
     * Apply rendered elements to RichEdit control
     */
    void ApplyToRichEdit(HWND hwndRichEdit, const std::vector<RenderedElement>& elements);
    
    /**
     * Extract plain text from HTML (strip tags)
     */
    static std::string StripHtml(const std::string& html);
    
    /**
     * Extract title from HTML
     */
    static std::string ExtractTitle(const std::string& html);
    
private:
    std::string RemoveTags(const std::string& html, const std::string& tagName);
    std::string ExtractTag(const std::string& html, const std::string& tagName, size_t& pos);
};

#endif // HTML_RENDERER_H
