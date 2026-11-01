# IR 查看器（`bind/viewer/`）

给**脚本作者**用的 API 查询工具：不用打开任何 C++ 头文件，就能查清 Ruby 侧到底暴露了什么。

数据源只有一个 —— 由 `bind/gen_api_json.py` 生成的 `bind/api_reference.json`。
本目录不参与编译，不产生任何 C++ 目标，`bind/CMakeLists.txt` 也不会 glob 到这里。

```
bind/api_reference.json ──┬──▶ index.html + config.js   浏览器 UI（类树 / 搜索 / 签名 / 未能导出项）
                          ├──▶ viewer.py       命令行查询（可被脚本或智能体调用）
                          └──▶ api_embed.js    file:// / 离线兜底快照（由 viewer.py embed 生成）
```

## 目录内容

| 文件 | 说明 |
| --- | --- |
| `index.html` | 单文件 UI，零依赖、零 CDN。左边类树，右边签名表，顶部全局搜索 |
| `config.js` | 部署配置：GitHub 上的 IR 地址与自动刷新间隔 |
| `viewer.py` | 命令行入口：`serve` / `embed` / `list` / `show` / `search` / `stats` |
| `api_embed.js` | 由 `viewer.py embed` 生成的兜底数据（`file://` / 离线用） |
| `README.md` | 本文件 |

## 数据来源

页面按下面的顺序寻找 IR，第一个可用的生效：

1. `?ir=<url>` 查询参数（临时预览某个分支）
2. `config.js` 里的 `irUrl`（默认是 GitHub 上本仓库的 `bind/api_reference.json`）
3. 页面同目录的 `api_reference.json`（服务器自己拉取文件的部署方式）
4. `../api_reference.json`（仓库内布局，`bind/` 的上一级）
5. `api_embed.js` 快照

在 `localhost` / `127.0.0.1` 下会先试本地文件，所以本地开发始终看到工作区的最新 IR；
部署到其他域名时先请求 GitHub。右上角徽标签会显示当前来源（`github` / `local` / `custom` / `embedded`）。

## 部署到服务器

查看器是纯静态页面，不需要后端。把 `index.html` 和 `config.js` 上传到任意静态站点即可：

```js
// config.js
window.URGE_VIEWER_CONFIG = {
  irUrl: "https://raw.githubusercontent.com/Admenri/urge/master/bind/api_reference.json",
  refreshSeconds: 0,   // 例如改成 300 则每 5 分钟重新拉取
};
```

- 仓库或分支变了，只改 `irUrl`。
- 想试某个分支而不用改文件：`index.html?ir=https://raw.githubusercontent.com/<owner>/<repo>/<branch>/bind/api_reference.json`。
- 服务器可以定期把 JSON 拉到页面同目录，`config.js` 里把 `irUrl` 留空即可只读本地文件。
- 所有请求都带 `cache=` 参数和 `no-store`，不会读到 CDN 上的旧 JSON；右上角 Reload 按钮可手动刷新。

## 两种打开方式

**推荐：起本地服务**（`localhost` 下优先读取工作区的 IR，改完头文件重跑一次生成器就能看到最新结果）

```bash
python bind/viewer/viewer.py serve          # 默认 127.0.0.1:8770，自动开浏览器
python bind/viewer/viewer.py serve --port 8800 --no-open
```

只绑定回环地址 —— 它会把 `bind/` 暴露成 HTTP，没有鉴权，不要对外。

**离线：双击 `index.html`**。此时浏览器禁止 `file://` 下的 `fetch`，页面会依次尝试
本地/远程 IR，最后退回 `api_embed.js` 里的快照。首次使用前要先落一份快照：

```bash
python bind/viewer/viewer.py embed
```

> 页面右上角会标出当前数据来源；看到 `embedded` 就说明读的是快照，
> 重新生成了 IR 之后记得重跑 `embed`。

## 命令行查询

```
python bind/viewer/viewer.py <command> [options]
```

| 命令 | 用途 |
| --- | --- |
| `list [--json]` | 列出全部类与模块（父类、成员数、来源头文件） |
| `show TARGET [--json]` | 打印一个类、模块或单个成员 |
| `search QUERY [--limit N] [--json]` | 按名字 / 签名 / 参数 / 类型检索成员 |
| `stats [--json]` | 汇总计数、未能导出项 |
| `serve [--port N] [--no-open]` | 起 HTTP 服务并打开 UI |
| `embed [--out PATH]` | 写出 `file://` 兜底快照 |

全局参数 `--ir PATH` 可换用别的 IR 文件。

### TARGET 写法

```
Sprite                 类或模块
Sprite#bitmap          实例方法 / 属性读取侧
Sprite#bitmap=         属性写入侧（与上一行是同一条 ATTR()）
Sprite.bitmap          类方法 / 类属性
Font.default_bold      静态属性
Input.press?           模块函数（模块侧一律用 `.`）
Table#[]=              字面名字里就带 `=` 的成员
```

### 例子

```bash
# Sprite 的完整 Ruby 面（构造、实例方法、属性，属性会标出写入侧）
python bind/viewer/viewer.py show Sprite

# 我该用哪个方法画图？搜一下
python bind/viewer/viewer.py search blt

# 机器可读，喂给别的脚本
python bind/viewer/viewer.py search bitmap --json | jq '.[].call'
python bind/viewer/viewer.py stats --json
```

退出码：`0` 成功，`1` 目标不存在（`show`/`search`），`2` 用法错误。可以直接串进脚本。

## 呈现规则

- **属性**是读写对，显示为读取侧 `Sprite#bitmap`，下一行给出写入侧 `Sprite#bitmap = value`。
  两条共用同一条 IR 记录。
- **构造**显示为 `Sprite.new(viewport = nil)`。有重载的类（`Bitmap`、`Color`、`Rect`、
  `Tone`、`Vector2/3/4`、`Viewport`、`Image`）会列出全部重载。
- **默认值**按脚本作者会写的字面量呈现：IR 里的 C++ 拼写 `nullptr` / `{}` / `255.f`
  分别显示为 `nil` / `[]`（或 `""`）/ `255.0`。未映射的形状原样保留，不做猜测。
- **未能导出**的声明单独用醒目样式列出，并给出原因（例如 `internal parameter type: glm::vec2`）。
- **`_dump` / `_load`** 由 `marshal` 标记合成；`initialize` / `initialize_copy` 在 Ruby 里是
  私有方法，UI 会注明。

## 与流水线的关系

```
core/*.h ──gen_api_json.py──▶ api_reference.json ──generate_binding.py──▶ binding_*.{h,cc}
                                      └──viewer.py / index.html（本目录，只读）
```

改完 `core/*.h` 之后的顺序：

```bash
python bind/gen_api_json.py        # 重建 IR
python bind/viewer/viewer.py embed # 如果要在 file:// 下看，刷新快照
python bind/generate_binding.py    # 重新生成胶水（可选，viewer 不依赖它）
```

查看器**只读** IR，从不改写 `core/`、`api_reference.json` 或任何生成物。
