/**
 * html_renderer.cpp
 * 
 * Simple HTML renderer implementation
 */

#include "html_renderer.h"
#include <richedit.h>
#include <algorithm>
#include <sstream>
#include <regex>

HtmlRenderer::HtmlRenderer() {
}

HtmlRenderer::~HtmlRenderer() {
}

/**
 * Strip HTML tags
 */
std::string HtmlRenderer::StripHtml(const std::string& html) {
    std::string result;
    bool inTag = false;
    bool inScript = false;
    bool inStyle = false;
    
    for (size_t i = 0; i < html.length(); i++) {
        // Check for script/style tags
        if (i + 7 < html.length() && html.substr(i, 7) == "<script") {
            inScript = true;
            inTag = true;
        }
        if (i + 6 < html.length() && html.substr(i, 6) == "<style") {
            inStyle = true;
            inTag = true;
        }
        if (i + 9 < html.length() && html.substr(i, 9) == "</script>") {
            inScript = false;
            inTag = false;
            i += 8;
            continue;
        }
        if (i + 8 < html.length() && html.substr(i, 8) == "</style>") {
            inStyle = false;
            inTag = false;
            i += 7;
            continue;
        }
        
        if (html[i] == '<') {
            inTag = true;
        } else if (html[i] == '>') {
            inTag = false;
        } else if (!inTag && !inScript && !inStyle) {
            result += html[i];
        }
    }
    
    // Decode HTML entities
    std::string decoded;
    for (size_t i = 0; i < result.length(); i++) {
        if (result[i] == '&') {
            if (result.substr(i, 4) == "&lt;") {
                decoded += '<';
                i += 3;
            } else if (result.substr(i, 4) == "&gt;") {
                decoded += '>';
                i += 3;
            } else if (result.substr(i, 5) == "&amp;") {
                decoded += '&';
                i += 4;
            } else if (result.substr(i, 6) == "&quot;") {
                decoded += '"';
                i += 5;
            } else if (result.substr(i, 6) == "&nbsp;") {
                decoded += ' ';
                i += 5;
            } else {
                decoded += result[i];
            }
        } else {
            decoded += result[i];
        }
    }
    
    return decoded;
}

/**
 * Extract title from HTML
 */
std::string HtmlRenderer::ExtractTitle(const std::string& html) {
    size_t titleStart = html.find("<title>");
    if (titleStart == std::string::npos) {
        titleStart = html.find("<TITLE>");
    }
    
    if (titleStart != std::string::npos) {
        titleStart += 7;
        size_t titleEnd = html.find("</title>", titleStart);
        if (titleEnd == std::string::npos) {
            titleEnd = html.find("</TITLE>", titleStart);
        }
        
        if (titleEnd != std::string::npos) {
            return StripHtml(html.substr(titleStart, titleEnd - titleStart));
        }
    }
    
    return "Sans titre";
}

/**
 * Simple HTML parser - converts HTML to rendered elements
 */
std::vector<RenderedElement> HtmlRenderer::Render(const std::string& html) {
    std::vector<RenderedElement> elements;
    
    // Remove scripts and styles
    std::string cleaned = html;
    
    // Simple tag detection
    size_t pos = 0;
    bool inTag = false;
    std::string currentTag;
    std::string currentText;
    
    // Parse HTML line by line for simplicity
    std::istringstream stream(cleaned);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Skip empty lines
        if (line.empty()) continue;
        
        // Check for headings
        if (line.find("<h1>") != std::string::npos || line.find("<H1>") != std::string::npos) {
            RenderedElement elem;
            elem.type = HtmlElementType::HEADING1;
            elem.text = StripHtml(line) + "\n";
            elem.bold = true;
            elem.fontSize = 24;
            elem.color = RGB(0, 0, 0);
            elements.push_back(elem);
        }
        else if (line.find("<h2>") != std::string::npos || line.find("<H2>") != std::string::npos) {
            RenderedElement elem;
            elem.type = HtmlElementType::HEADING2;
            elem.text = StripHtml(line) + "\n";
            elem.bold = true;
            elem.fontSize = 20;
            elem.color = RGB(0, 0, 0);
            elements.push_back(elem);
        }
        else if (line.find("<h3>") != std::string::npos || line.find("<H3>") != std::string::npos) {
            RenderedElement elem;
            elem.type = HtmlElementType::HEADING3;
            elem.text = StripHtml(line) + "\n";
            elem.bold = true;
            elem.fontSize = 18;
            elem.color = RGB(0, 0, 0);
            elements.push_back(elem);
        }
        else if (line.find("<a ") != std::string::npos || line.find("<A ") != std::string::npos) {
            // Extract link
            size_t hrefPos = line.find("href=");
            if (hrefPos == std::string::npos) hrefPos = line.find("HREF=");
            
            if (hrefPos != std::string::npos) {
                hrefPos += 5;
                if (line[hrefPos] == '"' || line[hrefPos] == '\'') hrefPos++;
                
                size_t hrefEnd = line.find_first_of("\"' >", hrefPos);
                std::string url = line.substr(hrefPos, hrefEnd - hrefPos);
                
                RenderedElement elem;
                elem.type = HtmlElementType::LINK;
                elem.text = StripHtml(line);
                elem.url = url;
                elem.color = RGB(0, 0, 255);
                elem.italic = false;
                elem.bold = false;
                elem.fontSize = 14;
                elements.push_back(elem);
            }
        }
        else if (line.find("<li>") != std::string::npos || line.find("<LI>") != std::string::npos) {
            RenderedElement elem;
            elem.type = HtmlElementType::LIST_ITEM;
            elem.text = "• " + StripHtml(line) + "\n";
            elem.color = RGB(0, 0, 0);
            elem.fontSize = 14;
            elements.push_back(elem);
        }
        else if (line.find("<p>") != std::string::npos || line.find("<P>") != std::string::npos ||
                 line.find("</p>") != std::string::npos || line.find("</P>") != std::string::npos) {
            std::string text = StripHtml(line);
            if (!text.empty()) {
                RenderedElement elem;
                elem.type = HtmlElementType::PARAGRAPH;
                elem.text = text + "\n\n";
                elem.color = RGB(0, 0, 0);
                elem.fontSize = 14;
                elements.push_back(elem);
            }
        }
        else if (line.find("<pre>") != std::string::npos || line.find("<PRE>") != std::string::npos) {
            RenderedElement elem;
            elem.type = HtmlElementType::PREFORMATTED;
            elem.text = StripHtml(line) + "\n";
            elem.color = RGB(64, 64, 64);
            elem.fontSize = 12;
            elements.push_back(elem);
        }
        else {
            // Regular text
            std::string text = StripHtml(line);
            if (!text.empty() && text.find_first_not_of(" \t\n\r") != std::string::npos) {
                RenderedElement elem;
                elem.type = HtmlElementType::TEXT;
                elem.text = text + "\n";
                elem.color = RGB(0, 0, 0);
                elem.fontSize = 14;
                elements.push_back(elem);
            }
        }
    }
    
    return elements;
}

/**
 * Apply rendered elements to RichEdit control with formatting
 */
void HtmlRenderer::ApplyToRichEdit(HWND hwndRichEdit, const std::vector<RenderedElement>& elements) {
    // Clear existing content
    SetWindowTextA(hwndRichEdit, "");
    
    // Build RTF content
    std::stringstream rtf;
    rtf << "{\\rtf1\\ansi\\deff0\n";
    rtf << "{\\fonttbl{\\f0\\fnil\\fcharset0 Segoe UI;}}\n";
    rtf << "{\\colortbl;\\red0\\green0\\blue0;\\red0\\green0\\blue255;\\red64\\green64\\blue64;}\n";
    
    for (const auto& elem : elements) {
        // Font size
        rtf << "\\fs" << (elem.fontSize * 2);  // RTF uses half-points
        
        // Color (1=black, 2=blue, 3=gray)
        if (elem.color == RGB(0, 0, 255)) {
            rtf << "\\cf2 ";
        } else if (elem.color == RGB(64, 64, 64)) {
            rtf << "\\cf3 ";
        } else {
            rtf << "\\cf1 ";
        }
        
        // Bold/Italic
        if (elem.bold) rtf << "\\b ";
        if (elem.italic) rtf << "\\i ";
        
        // Text
        rtf << elem.text;
        
        // Reset formatting
        if (elem.bold) rtf << "\\b0 ";
        if (elem.italic) rtf << "\\i0 ";
        rtf << "\\par\n";
    }
    
    rtf << "}";
    
    // Set RTF content
    SETTEXTEX setText;
    setText.flags = ST_DEFAULT;
    setText.codepage = CP_UTF8;
    
    std::string rtfStr = rtf.str();
    SendMessageA(hwndRichEdit, EM_SETTEXTEX, (WPARAM)&setText, (LPARAM)rtfStr.c_str());
}
