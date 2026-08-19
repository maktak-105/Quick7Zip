import base64
import os
import re


def bundle(output_dir=None):
    base_dir = os.path.dirname(__file__)
    with open(os.path.join(base_dir, "templates", "index.html"), encoding="utf-8") as stream:
        html = stream.read()
    with open(os.path.join(base_dir, "static", "css", "style.css"), encoding="utf-8") as stream:
        css = stream.read()
    with open(os.path.join(base_dir, "static", "js", "app.js"), encoding="utf-8") as stream:
        javascript = stream.read()
    with open(os.path.join(base_dir, "static", "img", "quick7zip-icon.png"), "rb") as stream:
        icon_data = base64.b64encode(stream.read()).decode("ascii")
    html = html.replace(
        'src="static/img/quick7zip-icon.png"',
        f'src="data:image/png;base64,{icon_data}"',
    )

    body_match = re.search(r"<body[^>]*>(.*)</body>", html, flags=re.DOTALL | re.IGNORECASE)
    if not body_match:
        raise RuntimeError("templates/index.html must contain a body element")
    bundled = f"""<!doctype html>
<html lang="ja">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Quick7Zip</title>
  <style>{css}</style>
</head>
<body>{body_match.group(1)}
  <script>{javascript}</script>
</body>
</html>
"""
    output_dir = output_dir or os.path.join(base_dir, "dist", "binary")
    os.makedirs(output_dir, exist_ok=True)
    target = os.path.join(output_dir, "index.html")
    with open(target, "w", encoding="utf-8", newline="\n") as stream:
        stream.write(bundled)
    print(f"[OK] Generated {target} ({len(bundled)} bytes)")


if __name__ == "__main__":
    bundle()
