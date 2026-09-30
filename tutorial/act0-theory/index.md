---
title: Act 0 · GUI 理论篇
---

<ChapterHero act="0" title="GUI 理论篇" subtitle="12 器官世界观 · 看穿任何框架的眼睛" motif="theory" count="10 篇" />

> 如果您之前是看我们 Win32 的一些使用指南，初步理解了 Windows 的GUI抽象的朋友，现在我们要做的事情就是重新对得起我们 anatomy_gui 这个名字——提炼我们学习的具体的API，走到更加一般化，和普遍化的GUI理论。

Act 0 是整本书的**世界观**。在动手解剖任何一具框架标本之前，我们先建立一张地图：所有 GUI 框架都在回答的同一组问题（12 个器官）、几道根本分叉（保留 vs 即时、命令 vs 声明 vs 响应）、以及那些让 demo 和真实软件之间隔着鸿沟的隐藏成本。

读完这一篇，您将拥有一双能看穿任何 GUI 框架的眼睛——往后碰到任何新框架，往这 12 个器官上一套，陌生感褪掉大半。

## 12 个器官，一具骨架

<div class="organ-grid">
  <OrganCard :organNumber="1" name="窗口与显示面" latin="Fenestra" oneLine="像素往哪画，谁给一块能被看见的领地" link="/act0-theory/01_the_twelve_organs" />
  <OrganCard :organNumber="2" name="事件循环" latin="Circulus Eventuum" oneLine="谁来等事情发生，把它分发出去" link="/act0-theory/05_event_loop" />
  <OrganCard :organNumber="3" name="输入系统" latin="Sensorium" oneLine="按键和鼠标怎么变成「点了什么」的语义" link="/act0-theory/07_ime_focus_text" />
  <OrganCard :organNumber="4" name="对象模型" latin="Id Objecti" oneLine="一个控件到底是什么，归谁管、何时死" link="/act0-theory/06_widget_is_an_object" />
  <OrganCard :organNumber="5" name="控件树" latin="Arbor Widgetium" oneLine="界面上那么多东西，谁是谁的子节点" link="/act0-theory/06_widget_is_an_object" />
  <OrganCard :organNumber="6" name="布局" latin="Dispositio" oneLine="每个控件摆在哪、多大，谁来算" link="/act0-theory/06_widget_is_an_object" />
  <OrganCard :organNumber="7" name="绘制与合成" latin="Pictura" oneLine="谁决定何时重画、谁把像素合成上屏" link="/act0-theory/02_retained_vs_immediate" />
  <OrganCard :organNumber="8" name="文本系统" latin="Textus" oneLine="为什么画一行字比画矩形难十倍" link="/act0-theory/07_ime_focus_text" />
  <OrganCard :organNumber="9" name="样式系统" latin="Stylus" oneLine="外观怎么从代码里摘出来、一键换皮" link="/act0-theory/09_hidden_costs" />
  <OrganCard :organNumber="10" name="状态绑定" latin="Vinculum" oneLine="数据变了，界面由谁、怎么跟着变" link="/act0-theory/03_imperative_declarative_reactive" />
  <OrganCard :organNumber="11" name="异步并发" latin="Concurrentia" oneLine="不能卡 UI，耗时活儿怎么回投主线程" link="/act0-theory/05_event_loop" />
  <OrganCard :organNumber="12" name="可访问性与交付" latin="Accessibilitas" oneLine="屏幕有像素 ≠ 谁都能用；demo ≠ 真东西" link="/act0-theory/08_accessibility" />
</div>

## 十篇导览

| 篇 | 标题 | 一句话 |
|:---:|:---|:---|
| T00 | [为什么 GUI 不是一份 API 清单](00_why_not_api_list) | API 是答案，不是问题 |
| T01 | [GUI 的 12 个器官](01_the_twelve_organs) | 一张所有框架共享的地图 |
| T02 | [保留模式 vs 即时模式](02_retained_vs_immediate) | 状态归谁管 |
| T03 | [命令式、声明式、响应式](03_imperative_declarative_reactive) | 你把多少活儿交给框架 |
| T04 | [MVC / MVP / MVVM / Elm Architecture](04_mvc_mvp_mvvm_tea) | 同一道题的四代解法 |
| T05 | [事件循环](05_event_loop) | GUI 的那颗心脏 |
| T06 | [控件不是矩形，而是对象](06_widget_is_an_object) | 身份、生命期、所有权 |
| T07 | [输入法、焦点、文本](07_ime_focus_text) | GUI 的深水区 |
| T08 | [可访问性不是附加功能](08_accessibility) | 默认义务，不是糖霜 |
| T09 | [GUI 的隐藏成本](09_hidden_costs) | demo 与真东西的鸿沟 |

## 接下来

理论篇讲完，就该下刀了——下一站是造骨架：先脱掉所有封装，用 Win32 徒手写一遍（具象），再把封装一层层长出来（抽象），最后落回同一个 demo（具象）。这条线正在重构中。
