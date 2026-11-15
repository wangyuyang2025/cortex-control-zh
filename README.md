# Cortex Control 中文版（非官方）

给 Neural DSP Quad Cortex 的电脑端软件 **Cortex Control** 做的中文化工具。它不分发任何 Neural DSP 的程序文件：脚本在你自己的电脑上，把你已经装好的官方 Cortex Control 复制一份，改成中文，官方原版一个字节都不动。

- 界面文字：中文，行业词保留英文（如「增益 GAIN」「音箱 AMP」）
- 箱头、箱体、效果器型号：直接显示真机名（`Brit 2203` → `Marshall JCM800`），同一台机器的不同通道带通道名
- 出厂预设名：同样显示真机名
- 设置页的说明文字：中文

> 本项目与 Neural DSP 无关，未获其授权或认可。Quad Cortex、Cortex Control 及文中出现的各品牌、型号名称均为其各自所有者的商标，这里只用于说明对应关系。使用风险自负。

**当前版本：v0.1.0**（适用官方 Cortex Control **4.1.0**，Apple 芯片）。更新记录见 [CHANGELOG.md](CHANGELOG.md)。

版本号怎么读：中文版自己的版本号（`VERSION` 文件）和它适用的官方版本号分开写。安装包文件名里两个都有，如 `Cortex-Control-zh_v0.1.0_CC4.1.0_mac-arm64.zip`。装好的中文版里也记着：`Info.plist` 的 `CortexZhVersion`（中文版版本）和 `CortexZhBasedOn`（官方版本）。

## 下载安装（不用装 Xcode）

到本仓库的 Releases 页下载和你官方版本号对应的 `Cortex-Control-zh_v…_CC…_mac-arm64.zip`，解压后双击「安装.command」。

- 第一次双击如果被系统拦住：「系统设置 → 隐私与安全性」里点「仍要打开」；或者打开终端，输入 `zsh ` 再把「安装.command」拖进去回车。
- 装好的中文版在 `~/Applications/Cortex Control 中文.app`。官方原版不动。
- 安装包里**没有** Neural DSP 的程序文件：只有翻译层插件、词典和一份「改动单」（我们的译文 + “从官方程序第几字节抄多少字节”的指令），安装时在你电脑上照单改你自己装的那一份，并先核对官方程序的 SHA-256，版本对不上就不动手。

## 能用的环境（自己从源码生成）

| 项目 | 要求 |
|---|---|
| 电脑 | Apple 芯片的 Mac（Intel 不支持） |
| Cortex Control | **4.1.0**，装在 `/Applications/Neural DSP/Cortex Control.app` |
| 工具 | Xcode 命令行工具（`xcode-select --install`）、系统自带的 python3 |

版本对不上时：型号真名、参数名这一层（翻译层）会自动不启用，程序照常能用；界面布局文字那一层按原文匹配，大多仍会变中文，但我没在别的版本上试过。

## 从源码生成

```bash
zsh 程序内汉化/打补丁.sh
```

生成的「Cortex Control 中文.app」默认在 `~/Applications`；想放别处：

```bash
CC_ZH_DEST=/你的/文件夹 zsh 程序内汉化/打补丁.sh
```

官方升级后重跑一次即可。想退回，直接开官方原版，或者把生成的中文版删掉。

关掉 Cortex Control 后请等六七秒再开，开太快 Quad Cortex 容易连不上。

## 改词

所有人工规则在 `程序内汉化/翻译层/词典覆盖.txt`，一行一条 `英文原文|显示文字`，改完重跑 `打补丁.sh`。
同一个英文词在不同地方要不同叫法（如 BASS 在标签页是「贝斯」、在旋钮上是「低频」），写成 `BASS@B|贝斯 BASS`，`@` 后面是「场合」标记，文件开头有说明。

界面布局里的固定文字在 `程序内汉化/译文覆盖.txt` 和 `程序内汉化/补充译文.txt`。

## 它是怎么做的

分两层：

1. **改布局文字**（`patch_xml.py`）。Cortex Control 是 JUCE 程序，界面布局是内置在程序里的 XML。脚本把里面的 `text=`、`tooltip=` 等换成中文，每份布局改完长度保持不变（译文变长的部分靠压缩缩进空白腾出来），再重新签名。
2. **翻译层**（`翻译层/hook.c`）。型号名是 Quad Cortex 通过 USB 发来的，参数名、左侧分类等是程序运行时现画的，布局里没有。翻译层是一个随中文版一起加载的小插件，启动时在程序「画文字」的四个函数入口各挂一道：文字显示前先查词典，查到就换。它还会看「是谁在画这个字」，所以同一个词在不同地方可以有不同译法，预设列表和型号列表也能分开处理。
   挂之前先核对这四处的机器码，对不上（版本不同）就什么都不做。

四个挂点（4.1.0，arm64，未滑动地址）：

| 地址 | 函数 | 管什么 |
|---|---|---|
| `0x1011b42d0` | `juce::Graphics::drawText(const String&, Rectangle<int>, …)` | 普通文字 |
| `0x1011b4e48` | `juce::Graphics::drawText(const String&, Rectangle<float>, …)` | 普通文字 |
| `0x1011b5904` | `juce::Graphics::drawFittedText(…)` | 会自动缩放的文字 |
| `0x1011d95f0` | `juce::AttributedString::append(const String&, const Font&, Colour)` | 几段不同颜色、粗细拼起来的文字（顶栏预设名、设置页说明） |

官方升级后地址会变，要重新找。找的办法和用过的工具在 `翻译层/找挂点_probe2.c`、`找挂点_probe3.c`；`翻译层/走不通的路/` 里是试过但不能用的两个位置，以及为什么不能用。

## 发新版（维护者）

1. 改 `VERSION`、在 `CHANGELOG.md` 加一节。
2. `zsh 安装包/做安装包.sh`（需要 python3 + numpy），安装包生成在 `发布/`。
3. 打标签 `v版本号`，把 zip 和 `.sha256` 传到 Releases。

官方 Cortex Control 升级后：先重找四个挂点，再发一个对应新官方版本号的安装包；旧安装包留着给没升级的人。

## 已知的限制

- 只支持 Apple 芯片、Cortex Control 4.1.0。
- 锁定设备页示意图上的两行小字在图片里，改不了。
- 词典大部分没有逐条校对；发现叫法不对的，改 `词典覆盖.txt` 或提 issue。
- 你自己存的预设如果名字和某个型号一模一样，也会显示成真机名（只是显示，预设本身没有被改名）。
- 中文版用的是本机临时签名，并且关闭了 hardened runtime（翻译层需要），所以它只适合在自己电脑上生成、自己用，不要把生成的 app 发给别人。

## 文件

```
程序内汉化/
  打补丁.sh            一键：复制官方版 → 改布局文字 → 装翻译层 → 签名
  patch_xml.py         改内置 XML 布局里的文字
  merge.py             合并各来源的译文，生成 译文.json
  译文覆盖.txt  补充译文.txt   布局文字的人工规则
  翻译层/
    hook.c             翻译层本体
    build_dict.py      生成词典 cczh_dict.tsv
    词典覆盖.txt        词典的人工规则（改词改这里）
    add_dylib.py       给程序加「启动时加载翻译层」的指令
    找挂点_probe*.c     找挂点用的工具
安装包/
  做安装包.sh          生成 Releases 里的安装包
  make_delta.py        比较官方程序和改好的程序，生成改动单
  applydelta.c         安装时照改动单改写的小工具
  安装.command         安装包里给用户双击的脚本
对照数据/               界面文字清单、型号对照、译文来源
VERSION  CHANGELOG.md   版本号、更新记录
```

## 致谢与来源

- 型号与真机的对应关系来自 Neural DSP 官方公开的 [Device List](https://neuraldsp.com/device-list)。
- 界面译文、参数名译文由 AI 辅助翻译，再由作者按实际界面逐处核对、人工定稿（场合区分、真机名、设置页说明等）。起步时参考过社区流传的 Windows 版汉化补丁里的 AI 译文（`对照数据/Windows补丁译文/`），没有照搬。叫法不对的欢迎提 issue。
- Mac 版的实现由本仓库作者在 Claude 协助下完成。

## 许可

本仓库的代码和译文按 MIT 许可发布，见 `LICENSE`。Quad Cortex、Cortex Control 及各品牌、型号名称归其各自所有者。
