# web_builder.py - Script per minificare e ottimizzare i file HTML, CSS e JS per l'interfaccia web dell'ESP32

import os
import re

def minify_code(content, file_type):
    if file_type == "html":
        # Rimuove commenti HTML
        content = re.sub(r'', '', content, flags=re.DOTALL)
        # Comprime spazi bianchi e rimpiazza con spazio singolo
        content = re.sub(r'\s+', ' ', content)
    elif file_type == "css":
        # Rimuove commenti CSS
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
        # Rimuove spazi attorno ai simboli speciali
        content = re.sub(r'\s*([{:;,])\s*', r'\1', content)
        content = re.sub(r'\s+', ' ', content)
    elif file_type == "js":
        # Rimuove commenti multi-riga
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
        # Rimuove commenti su riga singola (facendo attenzione a non rompere le URL)
        content = re.sub(r'(?<![:"\'/])//.*', '', content)
        # Comprime spazi
        content = re.sub(r'\s+', ' ', content)
    return content.strip()

def build_web():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    src_dir = os.path.join(base_dir, "components", "Esp32_WebInterface", "web_src")
    output_file = os.path.join(base_dir, "components", "Esp32_WebInterface", "index_html.h")

    try:
        # Lettura file originali
        with open(os.path.join(src_dir, "index.html"), "r", encoding="utf-8") as f:
            html = f.read()
        with open(os.path.join(src_dir, "style.css"), "r", encoding="utf-8") as f:
            css = f.read()
        with open(os.path.join(src_dir, "script.js"), "r", encoding="utf-8") as f:
            js = f.read()

        # 1. Minifica CSS e JS singolarmente
        css_min = minify_code(css, "css")
        js_min = minify_code(js, "js")
        
        # 2. Inserimento nei marcatori
        html = html.replace("/*CSS_HERE*/", css_min)
        html = html.replace("//JS_HERE", js_min)
        
        # 3. Minifica l'intero HTML risultante
        final_html = minify_code(html, "html")

        # Scrittura file .h
        with open(output_file, "w", encoding="utf-8") as f:
            f.write("/* GENERATED FILE - MINIFIED & OPTIMIZED */\n")
            f.write("#ifndef INDEX_HTML_H\n#define INDEX_HTML_H\n\n")
            f.write('const char index_html[] = R"rawliteral(\n')
            f.write(final_html)
            f.write('\n)rawliteral";\n\n#endif\n')

        print("SUCCESS: index_html.h generated and MINIFIED!")

    except Exception as e:
        print(f"ERROR: {str(e)}")
        exit(1)

if __name__ == "__main__":
    build_web()