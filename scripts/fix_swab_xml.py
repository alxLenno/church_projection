import re
import os

input_file = "/Users/lennoxkk/Documents/Bible reading/church-projection/assets/bible/SWAB.xml.bak"
output_file = "/Users/lennoxkk/Documents/Bible reading/church-projection/assets/bible/SWAB.xml"

print(f"Reading {input_file}...")
with open(input_file, 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

# Remove existing root tags to re-apply them correctly
if text.startswith('<bible>'):
    text = text[len('<bible>'):]
if text.endswith('</bible>'):
    text = text[:-len('</bible>')]

output = ['<?xml version="1.0" encoding="UTF-8"?>\n<bible>\n']

current_book = None
current_chapter = None

print("Parsing tokens...")
# Regex to split while keeping tags
tokens = re.split(r'(<[^>]+>)', text)

for token in tokens:
    if not token:
        continue
    
    if token.startswith('<'):
        tag_content = token[1:-1].strip()
        
        if tag_content.startswith('/'):
            # Closing tag
            tag_name = tag_content[1:].strip()
            if tag_name == 'v':
                output.append('</v>')
            # We ignore other original closing tags to re-generate them at proper levels
            continue
            
        # Verse tag: v n=1
        if tag_content.startswith('v '):
            m = re.match(r'v n=(\d+)', tag_content)
            if m:
                output.append(f'<v n="{m.group(1)}">')
            else:
                output.append(token)
                
        # Chapter tag: <1>
        elif tag_content.isdigit():
            if current_chapter:
                output.append('</c>\n')
            current_chapter = tag_content
            output.append(f'<c n="{current_chapter}">\n')
            
        # Book tag: <Mwanzo n=1>
        elif ' n=' in tag_content:
            m = re.match(r'(.+) n=\d+', tag_content)
            if m:
                if current_chapter:
                    output.append('</c>\n')
                    current_chapter = None
                if current_book:
                    output.append('</b>\n')
                current_book = m.group(1).strip()
                output.append(f'<b n="{current_book}">\n')
            else:
                output.append(token)
        else:
            # Catch-all for other tags (like <?xml ... ?> if it was inside)
            output.append(token)
    else:
        # Text content
        output.append(token)

# Close remaining tags
if current_chapter:
    output.append('</c>\n')
if current_book:
    output.append('</b>\n')
output.append('</bible>')

print(f"Writing fixed content to {output_file}...")
with open(output_file, 'w', encoding='utf-8') as f:
    f.write("".join(output))

print("Done.")
