import base64
import os
import re


def bundle(output_dir=None):
    script_dir = os.path.dirname(__file__)
    root_dir = os.path.dirname(script_dir)
    ui_dir = os.path.join(root_dir, "src", "ui")

    with open(os.path.join(ui_dir, "index.html"), encoding="utf-8") as stream:
        html = stream.read()
    with open(os.path.join(ui_dir, "css", "style.css"), encoding="utf-8") as stream:
        css = stream.read()
    with open(os.path.join(ui_dir, "js", "app.js"), encoding="utf-8") as stream:
        javascript = stream.read()
    with open(os.path.join(ui_dir, "img", "quick7zip-icon.png"), "rb") as stream:
        icon_data = base64.b64encode(stream.read()).decode("ascii")
    html = html.replace(
        'src="static/img/quick7zip-icon.png"',
        f'src="data:image/png;base64,{icon_data}"',
    )
    with open(os.path.join(root_dir, "assets", "maktak105-V04-01.jpg"), "rb") as stream:
        badge_data = base64.b64encode(stream.read()).decode("ascii")
    html = html.replace(
        'src="assets/maktak105-V04-01.jpg"',
        f'src="data:image/jpeg;base64,{badge_data}"',
    )

    body_match = re.search(r"<body[^>]*>(.*)</body>", html, flags=re.DOTALL | re.IGNORECASE)
    if not body_match:
        raise RuntimeError("src/ui/index.html must contain a body element")
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
    output_dir = output_dir or os.path.join(root_dir, "build", "intermediate")
    os.makedirs(output_dir, exist_ok=True)
    target = os.path.join(output_dir, "index.html")
    with open(target, "w", encoding="utf-8", newline="\n") as stream:
        stream.write(bundled)
    print(f"[OK] Generated {target} ({len(bundled)} bytes)")
    return target


if __name__ == "__main__":
    bundle()
