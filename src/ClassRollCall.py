# Powered by Jzr
import json
import os
import random
import sys
import csv
import shutil
import winreg as reg
import webbrowser
import math

from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from PyQt5.QtGui import *

class RippleEffect(QWidget):
    """波纹动画效果 - 简约风格"""
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setAttribute(Qt.WA_ShowWithoutActivating)
        
        screen = QApplication.primaryScreen()
        geometry = screen.geometry()
        self.setGeometry(geometry)
        
        self.animations = []
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_animations)
        self.timer.start(20)  # 50fps
        
        self.current_style = 'ripple'  # ripple, spark, circle
        
    def set_animation_style(self, style):
        """设置动画样式"""
        self.current_style = style
        
    def add_animation(self, pos):
        """添加动画效果"""
        center = QPoint(self.width() // 2, self.height() // 2)
        
        if self.current_style == 'ripple':
            # 简约波纹
            self.animations.append({
                'pos': center,
                'radius': 20,
                'max_radius': min(self.width(), self.height()) // 2,
                'alpha': 200,
                'width': 3,
                'growing': True
            })
        elif self.current_style == 'spark':
            # 星光效果
            for i in range(8):
                angle = i * 45
                self.animations.append({
                    'pos': center,
                    'angle': angle,
                    'length': 30,
                    'max_length': 150,
                    'alpha': 200,
                    'width': 2,
                    'growing': True,
                    'type': 'spark'
                })
        elif self.current_style == 'circle':
            # 旋转光环
            self.animations.append({
                'pos': center,
                'radius': 40,
                'angle': 0,
                'alpha': 180,
                'width': 4,
                'growing': True,
                'type': 'circle'
            })
        
    def update_animations(self):
        """更新动画"""
        for anim in self.animations[:]:
            if anim.get('type') == 'spark':
                # 星光效果更新
                if anim['growing']:
                    anim['length'] += 8
                    anim['alpha'] = max(0, anim['alpha'] - 3)
                    if anim['length'] >= anim['max_length'] or anim['alpha'] <= 0:
                        self.animations.remove(anim)
            elif anim.get('type') == 'circle':
                # 旋转光环更新
                if anim['growing']:
                    anim['radius'] += 5
                    anim['angle'] = (anim['angle'] + 10) % 360
                    anim['alpha'] = max(0, anim['alpha'] - 2)
                    if anim['radius'] >= min(self.width(), self.height()) // 2 or anim['alpha'] <= 0:
                        self.animations.remove(anim)
            else:
                # 波纹更新
                if anim['growing']:
                    anim['radius'] += 12
                    anim['alpha'] = max(0, anim['alpha'] - 3)
                    if anim['radius'] >= anim['max_radius'] or anim['alpha'] <= 0:
                        self.animations.remove(anim)
                
        if self.animations:
            self.update()
        else:
            self.hide()
            
    def paintEvent(self, event):
        if not self.animations:
            return
            
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        for anim in self.animations:
            if anim.get('type') == 'spark':
                # 绘制星光
                painter.save()
                painter.translate(anim['pos'])
                painter.rotate(anim['angle'])
                
                pen = QPen(QColor(255, 215, 0, anim['alpha']))
                pen.setWidth(anim['width'])
                painter.setPen(pen)
                
                painter.drawLine(0, 0, anim['length'], 0)
                painter.drawLine(0, 0, anim['length'] // 2, -anim['length'] // 3)
                painter.drawLine(0, 0, anim['length'] // 2, anim['length'] // 3)
                
                painter.restore()
                
            elif anim.get('type') == 'circle':
                # 绘制旋转光环
                painter.save()
                painter.translate(anim['pos'])
                painter.rotate(anim['angle'])
                
                pen = QPen(QColor(100, 200, 255, anim['alpha']))
                pen.setWidth(anim['width'])
                painter.setPen(pen)
                painter.setBrush(Qt.NoBrush)
                
                painter.drawEllipse(QPoint(0, 0), anim['radius'], anim['radius'] // 2)
                
                painter.restore()
            else:
                # 绘制波纹
                pen = QPen(QColor(100, 150, 255, anim['alpha']))
                pen.setWidth(anim['width'])
                painter.setPen(pen)
                painter.setBrush(Qt.NoBrush)
                painter.drawEllipse(anim['pos'], anim['radius'], anim['radius'])

class GlassPopup(QWidget):
    """毛玻璃效果弹窗 - 支持渐变和金属质感"""
    def __init__(self, name, settings, parent=None):
        super().__init__(parent)
        self.name = name
        self.settings = settings
        
        # 获取屏幕DPI缩放因子
        screen = QApplication.primaryScreen()
        dpi = screen.logicalDotsPerInch()
        scale_factor = dpi / 96.0
        
        # 获取屏幕分辨率
        screen_geometry = screen.geometry()
        self.screen_width = screen_geometry.width()
        self.screen_height = screen_geometry.height()
        
        # 获取字体设置
        font_family = self.settings.get('popup_font_family', 'Microsoft YaHei')
        
        # 使用固定的字体大小（物理尺寸），根据DPI缩放
        base_font_size = int(48 * scale_factor)
        temp_font = QFont(font_family, base_font_size, QFont.Bold)
        font_metrics = QFontMetrics(temp_font)
        text_width = font_metrics.horizontalAdvance(name)
        text_height = font_metrics.height()
        
        # 弹窗宽度：文本宽度 + 120像素边距（考虑DPI缩放）
        margin = int(120 * scale_factor)
        popup_width = int(text_width + margin)
        
        # 弹窗高度：文本高度的2.5倍
        popup_height = int(text_height * 2.5)
        
        # 设置最小和最大尺寸限制（考虑DPI缩放）
        min_width = int(300 * scale_factor)
        max_width = int(1200 * scale_factor)
        min_height = int(150 * scale_factor)
        max_height = int(400 * scale_factor)
        
        popup_width = max(min_width, min(popup_width, max_width))
        popup_height = max(min_height, min(popup_height, max_height))
        
        # 确保不超过屏幕的60%
        popup_width = min(popup_width, int(self.screen_width * 0.6))
        popup_height = min(popup_height, int(self.screen_height * 0.3))
        
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setFixedSize(popup_width, popup_height)
        
        # 居中显示
        self.move(self.screen_width//2 - popup_width//2, self.screen_height//2 - popup_height//2)
        
        # 加载背景图片（如果有）
        self.background_image = None
        if self.settings.get('popup_use_bg_image', False):
            bg_image_path = self.settings.get('popup_bg_image_path', '')
            if bg_image_path and os.path.exists(bg_image_path):
                self.background_image = QImage(bg_image_path)
        
        # 3秒后自动消失
        QTimer.singleShot(3000, self.close)
        
    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        # 获取屏幕DPI缩放因子
        screen = QApplication.primaryScreen()
        dpi = screen.logicalDotsPerInch()
        scale_factor = dpi / 96.0
        
        rect = self.rect()
        corner_radius = int(20 * scale_factor)
        
        # 获取背景样式
        bg_style = self.settings.get('popup_bg_style', 'gradient')
        bg_color1 = self.settings.get('popup_bg_color1', [100, 150, 255])
        bg_color2 = self.settings.get('popup_bg_color2', [200, 220, 255])
        bg_opacity = self.settings.get('popup_bg_opacity', 220)
        
        # 获取文字透明度
        text_opacity = self.settings.get('popup_text_opacity', 255)
        
        # 绘制背景
        if self.background_image and self.settings.get('popup_use_bg_image', False):
            # 绘制背景图片
            img_opacity = self.settings.get('popup_bg_image_opacity', 180)
            scaled_img = self.background_image.scaled(rect.size(), Qt.KeepAspectRatioByExpanding, Qt.SmoothTransformation)
            
            # 设置透明度
            painter.setOpacity(img_opacity / 255.0)
            
            # 计算裁剪区域以居中显示
            img_rect = scaled_img.rect()
            img_rect.moveCenter(rect.center())
            painter.drawImage(rect.topLeft(), scaled_img)
            
            # 恢复透明度并添加半透明遮罩
            painter.setOpacity(1.0)
            
            if bg_style == 'gradient':
                gradient = QLinearGradient(rect.topLeft(), rect.bottomRight())
                gradient.setColorAt(0, QColor(bg_color1[0], bg_color1[1], bg_color1[2], bg_opacity // 2))
                gradient.setColorAt(1, QColor(bg_color2[0], bg_color2[1], bg_color2[2], bg_opacity // 2))
                painter.setBrush(QBrush(gradient))
            elif bg_style == 'metal':
                # 金属质感
                gradient = QLinearGradient(rect.topLeft(), rect.bottomRight())
                gradient.setColorAt(0, QColor(220, 220, 220, bg_opacity // 2))
                gradient.setColorAt(0.5, QColor(255, 255, 255, bg_opacity // 2))
                gradient.setColorAt(1, QColor(180, 180, 180, bg_opacity // 2))
                painter.setBrush(QBrush(gradient))
            else:
                painter.setBrush(QBrush(QColor(bg_color1[0], bg_color1[1], bg_color1[2], bg_opacity // 2)))
            
            painter.setPen(Qt.NoPen)
            painter.drawRoundedRect(rect, corner_radius, corner_radius)
        else:
            # 绘制纯色背景
            painter.setPen(Qt.NoPen)
            
            if bg_style == 'gradient':
                gradient = QLinearGradient(rect.topLeft(), rect.bottomRight())
                gradient.setColorAt(0, QColor(bg_color1[0], bg_color1[1], bg_color1[2], bg_opacity))
                gradient.setColorAt(1, QColor(bg_color2[0], bg_color2[1], bg_color2[2], bg_opacity))
                painter.setBrush(QBrush(gradient))
            elif bg_style == 'metal':
                # 金属质感
                gradient = QLinearGradient(rect.topLeft(), rect.bottomRight())
                gradient.setColorAt(0, QColor(180, 180, 180, bg_opacity))
                gradient.setColorAt(0.5, QColor(240, 240, 240, bg_opacity))
                gradient.setColorAt(1, QColor(160, 160, 160, bg_opacity))
                painter.setBrush(QBrush(gradient))
            else:
                painter.setBrush(QBrush(QColor(bg_color1[0], bg_color1[1], bg_color1[2], bg_opacity)))
            
            painter.drawRoundedRect(rect, corner_radius, corner_radius)
        
        # 动态计算字体大小
        max_font_size = int(self.height() * 0.5)
        min_font_size = int(36 * scale_factor)
        
        # 获取字体设置
        font_family = self.settings.get('popup_font_family', 'Microsoft YaHei')
        
        font_size = max_font_size
        font = QFont(font_family, font_size, QFont.Bold)
        font_metrics = QFontMetrics(font)
        
        text_width = font_metrics.horizontalAdvance(self.name)
        available_width = self.width() - int(80 * scale_factor)
        
        while text_width > available_width and font_size > min_font_size:
            font_size -= 2
            font.setPointSize(font_size)
            font_metrics = QFontMetrics(font)
            text_width = font_metrics.horizontalAdvance(self.name)
        
        # 获取文字颜色
        text_color = self.settings.get('popup_text_color', [255,255,255])
        
        # 绘制名字（带透明度）
        painter.setPen(QColor(text_color[0], text_color[1], text_color[2], text_opacity))
        painter.setFont(font)
        
        text_x = (self.width() - text_width) // 2
        text_y = (self.height() + font_metrics.ascent() - font_metrics.descent()) // 2
        
        painter.drawText(text_x, text_y, self.name)
        
    def mousePressEvent(self, event):
        self.close()

class SettingsDialog(QDialog):
    """设置对话框 - 分类管理（带滚动功能）"""
    def __init__(self, parent=None):
        super().__init__(parent)
        self.parent = parent
        self.setWindowTitle("设置")
        self.setFixedSize(650, 650)
        
        # 设置窗口标志，移除问号按钮
        self.setWindowFlags(self.windowFlags() & ~Qt.WindowContextHelpButtonHint)
        
        # 设置窗口图标
        self.setWindowIcon(self.parent.windowIcon())
        
        # 加载当前设置
        self.settings = self.parent.settings.copy()
        
        self.setup_ui()
        self.load_settings()
        
    def setup_ui(self):
        main_layout = QVBoxLayout()
        
        # 创建选项卡
        self.tab_widget = QTabWidget()
        
        # 添加各个设置页面（带滚动）
        self.setup_general_tab()      # 常规设置
        self.setup_button_tab()       # 按钮设置
        self.setup_popup_tab()        # 弹窗设置
        
        main_layout.addWidget(self.tab_widget)
        
        # 底部按钮
        btn_layout = QHBoxLayout()
        
        self.restore_all_btn = QPushButton("恢复所有默认设置")
        self.restore_all_btn.clicked.connect(self.restore_all_defaults)
        
        btn_layout.addWidget(self.restore_all_btn)
        btn_layout.addStretch()
        
        save_btn = QPushButton("保存")
        cancel_btn = QPushButton("取消")
        save_btn.clicked.connect(self.save_settings)
        cancel_btn.clicked.connect(self.reject)
        
        btn_layout.addWidget(save_btn)
        btn_layout.addWidget(cancel_btn)
        
        main_layout.addLayout(btn_layout)
        
        self.setLayout(main_layout)
        
    def setup_general_tab(self):
        """常规设置页面（带滚动）"""
        tab = QWidget()
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        
        content = QWidget()
        layout = QVBoxLayout(content)
        
        # 开机自启动
        self.auto_start_cb = QCheckBox("开机自启动")
        layout.addWidget(self.auto_start_cb)
        
        # 抽取模式
        mode_group = QGroupBox("抽取模式")
        mode_layout = QVBoxLayout()
        
        self.mode_default_rb = QRadioButton("默认模式（抽完一轮后重新洗牌）")
        self.mode_random_rb = QRadioButton("真随机模式（每次抽取后重新洗牌）")
        
        mode_layout.addWidget(self.mode_default_rb)
        mode_layout.addWidget(self.mode_random_rb)
        
        mode_group.setLayout(mode_layout)
        layout.addWidget(mode_group)
        
        # 动画效果
        animation_group = QGroupBox("动画效果")
        animation_layout = QVBoxLayout()
        
        self.animation_combo = QComboBox()
        self.animation_combo.addItems(["简约波纹", "星光效果", "旋转光环", "无动画"])
        
        animation_layout.addWidget(QLabel("选择动画样式:"))
        animation_layout.addWidget(self.animation_combo)
        
        animation_group.setLayout(animation_layout)
        layout.addWidget(animation_group)
        
        # 名单文件
        file_group = QGroupBox("名单文件")
        file_layout = QHBoxLayout()
        
        self.file_path_edit = QLineEdit()
        self.file_path_edit.setPlaceholderText("选择CSV文件...")
        self.file_path_edit.setReadOnly(True)
        
        self.select_file_btn = QPushButton("浏览...")
        self.select_file_btn.clicked.connect(self.select_names_file)
        
        self.restore_file_btn = QPushButton("恢复默认")
        self.restore_file_btn.clicked.connect(self.restore_default_file)
        
        file_layout.addWidget(self.file_path_edit)
        file_layout.addWidget(self.select_file_btn)
        file_layout.addWidget(self.restore_file_btn)
        
        file_group.setLayout(file_layout)
        layout.addWidget(file_group)
        
        # 按钮位置重置
        position_group = QGroupBox("按钮位置")
        position_layout = QHBoxLayout()
        
        self.reset_position_btn = QPushButton("重置按钮位置到屏幕右下角")
        self.reset_position_btn.clicked.connect(self.reset_button_position)
        
        position_layout.addWidget(self.reset_position_btn)
        
        position_group.setLayout(position_layout)
        layout.addWidget(position_group)
        
        layout.addStretch()
        
        scroll.setWidget(content)
        tab_layout = QVBoxLayout(tab)
        tab_layout.addWidget(scroll)
        tab.setLayout(tab_layout)
        
        self.tab_widget.addTab(tab, "常规")
        
    def setup_button_tab(self):
        """按钮设置页面（带滚动）"""
        tab = QWidget()
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        
        content = QWidget()
        layout = QVBoxLayout(content)
        
        # 按钮文字
        text_group = QGroupBox("按钮文字")
        text_layout = QHBoxLayout()
        
        self.button_text_edit = QLineEdit()
        self.button_text_edit.setPlaceholderText("输入按钮显示文字...")
        
        self.restore_text_btn = QPushButton("恢复默认")
        self.restore_text_btn.clicked.connect(lambda: self.button_text_edit.setText("Call"))
        
        text_layout.addWidget(self.button_text_edit)
        text_layout.addWidget(self.restore_text_btn)
        
        text_group.setLayout(text_layout)
        layout.addWidget(text_group)
        
        # 按钮透明度
        opacity_group = QGroupBox("按钮透明度")
        opacity_layout = QHBoxLayout()
        
        self.opacity_slider = QSlider(Qt.Horizontal)
        self.opacity_slider.setRange(30, 255)
        self.opacity_slider.setTickInterval(15)
        self.opacity_slider.setTickPosition(QSlider.TicksBelow)
        self.opacity_value = QLabel("180")
        
        self.restore_opacity_btn = QPushButton("恢复默认")
        self.restore_opacity_btn.clicked.connect(lambda: self.opacity_slider.setValue(180))
        
        opacity_layout.addWidget(self.opacity_slider)
        opacity_layout.addWidget(self.opacity_value)
        opacity_layout.addWidget(self.restore_opacity_btn)
        
        opacity_group.setLayout(opacity_layout)
        layout.addWidget(opacity_group)
        
        # 按钮样式
        style_group = QGroupBox("按钮样式")
        style_layout = QVBoxLayout()
        
        self.button_style_combo = QComboBox()
        self.button_style_combo.addItems(["纯色", "渐变色", "金属质感"])
        self.button_style_combo.currentTextChanged.connect(self.on_button_style_changed)
        
        style_layout.addWidget(QLabel("选择样式:"))
        style_layout.addWidget(self.button_style_combo)
        
        # 颜色1
        color1_layout = QHBoxLayout()
        color1_layout.addWidget(QLabel("颜色1:"))
        self.color_preview1 = QLabel()
        self.color_preview1.setFixedSize(40, 25)
        self.color_preview1.setStyleSheet("background-color: rgb(100, 150, 255); border: 1px solid gray;")
        
        self.color_btn1 = QPushButton("选择")
        self.color_btn1.clicked.connect(lambda: self.choose_button_color(1))
        
        color1_layout.addWidget(self.color_preview1)
        color1_layout.addWidget(self.color_btn1)
        color1_layout.addStretch()
        style_layout.addLayout(color1_layout)
        
        # 颜色2（用于渐变）
        color2_layout = QHBoxLayout()
        color2_layout.addWidget(QLabel("颜色2:"))
        self.color_preview2 = QLabel()
        self.color_preview2.setFixedSize(40, 25)
        self.color_preview2.setStyleSheet("background-color: rgb(200, 100, 255); border: 1px solid gray;")
        
        self.color_btn2 = QPushButton("选择")
        self.color_btn2.clicked.connect(lambda: self.choose_button_color(2))
        
        color2_layout.addWidget(self.color_preview2)
        color2_layout.addWidget(self.color_btn2)
        color2_layout.addStretch()
        style_layout.addLayout(color2_layout)
        
        # 恢复默认按钮
        restore_color_layout = QHBoxLayout()
        self.restore_color_btn = QPushButton("恢复默认颜色")
        self.restore_color_btn.clicked.connect(self.restore_button_color)
        restore_color_layout.addWidget(self.restore_color_btn)
        style_layout.addLayout(restore_color_layout)
        
        style_group.setLayout(style_layout)
        layout.addWidget(style_group)
        
        layout.addStretch()
        
        scroll.setWidget(content)
        tab_layout = QVBoxLayout(tab)
        tab_layout.addWidget(scroll)
        tab.setLayout(tab_layout)
        
        self.tab_widget.addTab(tab, "按钮")
        
    def setup_popup_tab(self):
        """弹窗设置页面（带滚动）"""
        tab = QWidget()
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        
        content = QWidget()
        layout = QVBoxLayout(content)
        
        # 字体选择
        font_group = QGroupBox("显示字体")
        font_layout = QVBoxLayout()
        
        self.font_combo = QFontComboBox()
        self.font_combo.setFontFilters(QFontComboBox.AllFonts)
        
        font_layout.addWidget(self.font_combo)
        
        self.restore_font_btn = QPushButton("恢复默认")
        self.restore_font_btn.clicked.connect(lambda: self.font_combo.setCurrentFont(QFont("Microsoft YaHei")))
        font_layout.addWidget(self.restore_font_btn)
        
        font_group.setLayout(font_layout)
        layout.addWidget(font_group)
        
        # 文字颜色
        text_color_group = QGroupBox("文字颜色")
        text_color_layout = QHBoxLayout()
        
        self.text_color_preview = QLabel()
        self.text_color_preview.setFixedSize(50, 30)
        self.text_color_preview.setStyleSheet("background-color: rgb(255, 255, 255); border: 1px solid gray;")
        
        self.text_color_btn = QPushButton("选择颜色")
        self.text_color_btn.clicked.connect(self.choose_text_color)
        
        self.restore_text_color_btn = QPushButton("恢复默认")
        self.restore_text_color_btn.clicked.connect(self.restore_text_color)
        
        text_color_layout.addWidget(self.text_color_preview)
        text_color_layout.addWidget(self.text_color_btn)
        text_color_layout.addWidget(self.restore_text_color_btn)
        text_color_layout.addStretch()
        
        text_color_group.setLayout(text_color_layout)
        layout.addWidget(text_color_group)
        
        # 文字透明度
        text_opacity_group = QGroupBox("文字透明度")
        text_opacity_layout = QHBoxLayout()
        
        self.text_opacity_slider = QSlider(Qt.Horizontal)
        self.text_opacity_slider.setRange(30, 255)
        self.text_opacity_slider.setValue(255)
        self.text_opacity_slider.setTickInterval(15)
        self.text_opacity_slider.setTickPosition(QSlider.TicksBelow)
        self.text_opacity_value = QLabel("255")
        
        self.restore_text_opacity_btn = QPushButton("恢复默认")
        self.restore_text_opacity_btn.clicked.connect(lambda: self.text_opacity_slider.setValue(255))
        
        text_opacity_layout.addWidget(self.text_opacity_slider)
        text_opacity_layout.addWidget(self.text_opacity_value)
        text_opacity_layout.addWidget(self.restore_text_opacity_btn)
        
        text_opacity_group.setLayout(text_opacity_layout)
        layout.addWidget(text_opacity_group)
        
        # 弹窗背景样式
        bg_style_group = QGroupBox("弹窗背景样式")
        bg_style_layout = QVBoxLayout()
        
        self.popup_style_combo = QComboBox()
        self.popup_style_combo.addItems(["渐变色", "纯色", "金属质感"])
        self.popup_style_combo.currentTextChanged.connect(self.on_popup_style_changed)
        
        bg_style_layout.addWidget(QLabel("选择样式:"))
        bg_style_layout.addWidget(self.popup_style_combo)
        
        # 背景颜色1
        bg_color1_layout = QHBoxLayout()
        bg_color1_layout.addWidget(QLabel("颜色1:"))
        self.bg_color_preview1 = QLabel()
        self.bg_color_preview1.setFixedSize(40, 25)
        self.bg_color_preview1.setStyleSheet("background-color: rgb(100, 150, 255); border: 1px solid gray;")
        
        self.bg_color_btn1 = QPushButton("选择")
        self.bg_color_btn1.clicked.connect(lambda: self.choose_bg_color(1))
        
        bg_color1_layout.addWidget(self.bg_color_preview1)
        bg_color1_layout.addWidget(self.bg_color_btn1)
        bg_color1_layout.addStretch()
        bg_style_layout.addLayout(bg_color1_layout)
        
        # 背景颜色2
        bg_color2_layout = QHBoxLayout()
        bg_color2_layout.addWidget(QLabel("颜色2:"))
        self.bg_color_preview2 = QLabel()
        self.bg_color_preview2.setFixedSize(40, 25)
        self.bg_color_preview2.setStyleSheet("background-color: rgb(200, 100, 255); border: 1px solid gray;")
        
        self.bg_color_btn2 = QPushButton("选择")
        self.bg_color_btn2.clicked.connect(lambda: self.choose_bg_color(2))
        
        bg_color2_layout.addWidget(self.bg_color_preview2)
        bg_color2_layout.addWidget(self.bg_color_btn2)
        bg_color2_layout.addStretch()
        bg_style_layout.addLayout(bg_color2_layout)
        
        # 恢复默认背景颜色
        restore_bg_color_btn = QPushButton("恢复默认颜色")
        restore_bg_color_btn.clicked.connect(self.restore_bg_color)
        bg_style_layout.addWidget(restore_bg_color_btn)
        
        bg_style_group.setLayout(bg_style_layout)
        layout.addWidget(bg_style_group)
        
        # 背景透明度
        bg_opacity_group = QGroupBox("背景透明度")
        bg_opacity_layout = QHBoxLayout()
        
        self.bg_opacity_slider = QSlider(Qt.Horizontal)
        self.bg_opacity_slider.setRange(30, 255)
        self.bg_opacity_slider.setValue(220)
        self.bg_opacity_slider.setTickInterval(15)
        self.bg_opacity_slider.setTickPosition(QSlider.TicksBelow)
        self.bg_opacity_value = QLabel("220")
        
        self.restore_bg_opacity_btn = QPushButton("恢复默认")
        self.restore_bg_opacity_btn.clicked.connect(lambda: self.bg_opacity_slider.setValue(220))
        
        bg_opacity_layout.addWidget(self.bg_opacity_slider)
        bg_opacity_layout.addWidget(self.bg_opacity_value)
        bg_opacity_layout.addWidget(self.restore_bg_opacity_btn)
        
        bg_opacity_group.setLayout(bg_opacity_layout)
        layout.addWidget(bg_opacity_group)
        
        # 背景图片
        bg_image_group = QGroupBox("背景图片")
        bg_image_layout = QVBoxLayout()
        
        # 启用图片
        self.use_bg_image_cb = QCheckBox("使用背景图片")
        bg_image_layout.addWidget(self.use_bg_image_cb)
        
        # 图片路径
        image_path_layout = QHBoxLayout()
        self.bg_image_path_edit = QLineEdit()
        self.bg_image_path_edit.setPlaceholderText("选择图片文件...")
        self.bg_image_path_edit.setReadOnly(True)
        
        self.select_image_btn = QPushButton("浏览...")
        self.select_image_btn.clicked.connect(self.select_bg_image)
        
        self.clear_image_btn = QPushButton("清除")
        self.clear_image_btn.clicked.connect(self.clear_bg_image)
        
        image_path_layout.addWidget(self.bg_image_path_edit)
        image_path_layout.addWidget(self.select_image_btn)
        image_path_layout.addWidget(self.clear_image_btn)
        
        bg_image_layout.addLayout(image_path_layout)
        
        # 图片透明度
        image_opacity_layout = QHBoxLayout()
        image_opacity_layout.addWidget(QLabel("图片透明度:"))
        
        self.image_opacity_slider = QSlider(Qt.Horizontal)
        self.image_opacity_slider.setRange(30, 255)
        self.image_opacity_slider.setValue(180)
        self.image_opacity_slider.setTickInterval(15)
        self.image_opacity_slider.setTickPosition(QSlider.TicksBelow)
        self.image_opacity_value = QLabel("180")
        
        image_opacity_layout.addWidget(self.image_opacity_slider)
        image_opacity_layout.addWidget(self.image_opacity_value)
        image_opacity_layout.addStretch()
        
        bg_image_layout.addLayout(image_opacity_layout)
        
        bg_image_group.setLayout(bg_image_layout)
        layout.addWidget(bg_image_group)
        
        layout.addStretch()
        
        scroll.setWidget(content)
        tab_layout = QVBoxLayout(tab)
        tab_layout.addWidget(scroll)
        tab.setLayout(tab_layout)
        
        self.tab_widget.addTab(tab, "弹窗")
        
    def on_button_style_changed(self, style):
        """按钮样式改变时更新UI"""
        is_gradient_or_metal = style in ["渐变色", "金属质感"]
        self.color_btn2.setEnabled(is_gradient_or_metal)
        self.color_preview2.setEnabled(is_gradient_or_metal)
        
    def on_popup_style_changed(self, style):
        """弹窗样式改变时更新UI"""
        is_gradient_or_metal = style in ["渐变色", "金属质感"]
        self.bg_color_btn2.setEnabled(is_gradient_or_metal)
        self.bg_color_preview2.setEnabled(is_gradient_or_metal)
        
    def select_names_file(self):
        """选择名单文件"""
        file_path, _ = QFileDialog.getOpenFileName(
            self, "选择名单文件", 
            os.path.expanduser("~"),
            "CSV文件 (*.csv);;所有文件 (*.*)"
        )
        if file_path:
            self.file_path_edit.setText(file_path)
            self.settings['custom_names_file'] = file_path
            
    def restore_default_file(self):
        """恢复默认名单文件"""
        self.file_path_edit.clear()
        if 'custom_names_file' in self.settings:
            del self.settings['custom_names_file']
            
    def reset_button_position(self):
        """重置按钮位置到屏幕右下角"""
        self.parent.move_to_default_position()
        QMessageBox.information(self, "提示", "按钮位置已重置")
            
    def choose_button_color(self, color_num):
        """选择按钮颜色"""
        current_color = self.settings.get(f'button_color{color_num}', [100, 150, 255] if color_num == 1 else [200, 100, 255])
        color = QColorDialog.getColor(QColor(*current_color), self, f"选择按钮颜色{color_num}")
        if color.isValid():
            self.settings[f'button_color{color_num}'] = [color.red(), color.green(), color.blue()]
            preview = getattr(self, f'color_preview{color_num}')
            preview.setStyleSheet(f"background-color: rgb({color.red()}, {color.green()}, {color.blue()}); border: 1px solid gray;")
            
    def choose_text_color(self):
        """选择文字颜色"""
        color = QColorDialog.getColor(QColor(255, 255, 255), self, "选择文字颜色")
        if color.isValid():
            self.settings['popup_text_color'] = [color.red(), color.green(), color.blue()]
            self.text_color_preview.setStyleSheet(f"background-color: rgb({color.red()}, {color.green()}, {color.blue()}); border: 1px solid gray;")
            
    def choose_bg_color(self, color_num):
        """选择背景颜色"""
        current_color = self.settings.get(f'popup_bg_color{color_num}', [100, 150, 255] if color_num == 1 else [200, 100, 255])
        color = QColorDialog.getColor(QColor(*current_color), self, f"选择背景颜色{color_num}")
        if color.isValid():
            self.settings[f'popup_bg_color{color_num}'] = [color.red(), color.green(), color.blue()]
            preview = getattr(self, f'bg_color_preview{color_num}')
            preview.setStyleSheet(f"background-color: rgb({color.red()}, {color.green()}, {color.blue()}); border: 1px solid gray;")
            
    def restore_button_color(self):
        """恢复按钮默认颜色"""
        self.settings['button_color1'] = [100, 150, 255]
        self.settings['button_color2'] = [200, 100, 255]
        self.color_preview1.setStyleSheet("background-color: rgb(100, 150, 255); border: 1px solid gray;")
        self.color_preview2.setStyleSheet("background-color: rgb(200, 100, 255); border: 1px solid gray;")
        
    def restore_text_color(self):
        """恢复文字默认颜色"""
        self.settings['popup_text_color'] = [255, 255, 255]
        self.text_color_preview.setStyleSheet("background-color: rgb(255, 255, 255); border: 1px solid gray;")
        
    def restore_bg_color(self):
        """恢复背景默认颜色"""
        self.settings['popup_bg_color1'] = [100, 150, 255]
        self.settings['popup_bg_color2'] = [200, 100, 255]
        self.bg_color_preview1.setStyleSheet("background-color: rgb(100, 150, 255); border: 1px solid gray;")
        self.bg_color_preview2.setStyleSheet("background-color: rgb(200, 100, 255); border: 1px solid gray;")
        
    def select_bg_image(self):
        """选择背景图片"""
        file_path, _ = QFileDialog.getOpenFileName(
            self, "选择背景图片", 
            os.path.expanduser("~"),
            "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif);;所有文件 (*.*)"
        )
        if file_path:
            self.bg_image_path_edit.setText(file_path)
            self.settings['popup_bg_image_path'] = file_path
            self.use_bg_image_cb.setChecked(True)
            
    def clear_bg_image(self):
        """清除背景图片"""
        self.bg_image_path_edit.clear()
        self.use_bg_image_cb.setChecked(False)
        if 'popup_bg_image_path' in self.settings:
            del self.settings['popup_bg_image_path']
            
    def restore_all_defaults(self):
        """恢复所有默认设置"""
        reply = QMessageBox.question(
            self, "确认", 
            "确定要恢复所有默认设置吗？",
            QMessageBox.Yes | QMessageBox.No
        )
        
        if reply == QMessageBox.Yes:
            # 常规设置
            self.auto_start_cb.setChecked(False)
            self.mode_default_rb.setChecked(True)
            self.file_path_edit.clear()
            self.animation_combo.setCurrentIndex(0)  # 简约波纹
            
            # 按钮设置
            self.button_text_edit.setText("Call")
            self.opacity_slider.setValue(180)
            self.button_style_combo.setCurrentText("渐变色")  # 默认渐变色
            self.restore_button_color()
            
            # 弹窗设置
            self.font_combo.setCurrentFont(QFont("Microsoft YaHei"))
            self.restore_text_color()
            self.text_opacity_slider.setValue(255)  # 文字透明度默认255
            self.popup_style_combo.setCurrentText("渐变色")  # 默认渐变色
            self.restore_bg_color()
            self.bg_opacity_slider.setValue(220)
            self.use_bg_image_cb.setChecked(False)
            self.bg_image_path_edit.clear()
            self.image_opacity_slider.setValue(180)
            
            # 清除相关设置
            for key in ['custom_names_file', 'popup_bg_image_path']:
                if key in self.settings:
                    del self.settings[key]
                    
    def load_settings(self):
        """加载当前设置到界面"""
        # 常规设置
        self.auto_start_cb.setChecked(self.settings.get('auto_start', False))
        
        mode = self.settings.get('pick_mode', 'default')
        if mode == 'default':
            self.mode_default_rb.setChecked(True)
        else:
            self.mode_random_rb.setChecked(True)
            
        custom_file = self.settings.get('custom_names_file', '')
        if custom_file and os.path.exists(custom_file):
            self.file_path_edit.setText(custom_file)
            
        # 动画效果
        animation_map = {'ripple': 0, 'spark': 1, 'circle': 2, 'none': 3}
        current_anim = self.settings.get('animation_style', 'ripple')
        self.animation_combo.setCurrentIndex(animation_map.get(current_anim, 0))
        
        # 按钮设置
        self.button_text_edit.setText(self.settings.get('button_text', 'Call'))
        
        opacity = self.settings.get('button_opacity', 180)
        self.opacity_slider.setValue(opacity)
        self.opacity_value.setText(str(opacity))
        
        # 按钮样式
        button_style = self.settings.get('button_style', 'gradient')
        style_map = {'solid': '纯色', 'gradient': '渐变色', 'metal': '金属质感'}
        self.button_style_combo.setCurrentText(style_map.get(button_style, '渐变色'))
        
        color1 = self.settings.get('button_color1', [100, 150, 255])
        color2 = self.settings.get('button_color2', [200, 100, 255])
        self.color_preview1.setStyleSheet(f"background-color: rgb({color1[0]}, {color1[1]}, {color1[2]}); border: 1px solid gray;")
        self.color_preview2.setStyleSheet(f"background-color: rgb({color2[0]}, {color2[1]}, {color2[2]}); border: 1px solid gray;")
        
        # 弹窗设置
        font_family = self.settings.get('popup_font_family', 'Microsoft YaHei')
        self.font_combo.setCurrentFont(QFont(font_family))
        
        text_color = self.settings.get('popup_text_color', [255, 255, 255])
        self.text_color_preview.setStyleSheet(f"background-color: rgb({text_color[0]}, {text_color[1]}, {text_color[2]}); border: 1px solid gray;")
        
        # 文字透明度
        text_opacity = self.settings.get('popup_text_opacity', 255)
        self.text_opacity_slider.setValue(text_opacity)
        self.text_opacity_value.setText(str(text_opacity))
        
        # 弹窗背景样式
        popup_style = self.settings.get('popup_bg_style', 'gradient')
        style_map = {'solid': '纯色', 'gradient': '渐变色', 'metal': '金属质感'}
        self.popup_style_combo.setCurrentText(style_map.get(popup_style, '渐变色'))
        
        bg_color1 = self.settings.get('popup_bg_color1', [100, 150, 255])
        bg_color2 = self.settings.get('popup_bg_color2', [200, 100, 255])
        self.bg_color_preview1.setStyleSheet(f"background-color: rgb({bg_color1[0]}, {bg_color1[1]}, {bg_color1[2]}); border: 1px solid gray;")
        self.bg_color_preview2.setStyleSheet(f"background-color: rgb({bg_color2[0]}, {bg_color2[1]}, {bg_color2[2]}); border: 1px solid gray;")
        
        bg_opacity = self.settings.get('popup_bg_opacity', 220)
        self.bg_opacity_slider.setValue(bg_opacity)
        self.bg_opacity_value.setText(str(bg_opacity))
        
        use_bg_image = self.settings.get('popup_use_bg_image', False)
        self.use_bg_image_cb.setChecked(use_bg_image)
        
        bg_image_path = self.settings.get('popup_bg_image_path', '')
        if bg_image_path and os.path.exists(bg_image_path):
            self.bg_image_path_edit.setText(bg_image_path)
            
        image_opacity = self.settings.get('popup_bg_image_opacity', 180)
        self.image_opacity_slider.setValue(image_opacity)
        self.image_opacity_value.setText(str(image_opacity))
        
        # 连接信号
        self.opacity_slider.valueChanged.connect(lambda v: self.opacity_value.setText(str(v)))
        self.text_opacity_slider.valueChanged.connect(lambda v: self.text_opacity_value.setText(str(v)))
        self.bg_opacity_slider.valueChanged.connect(lambda v: self.bg_opacity_value.setText(str(v)))
        self.image_opacity_slider.valueChanged.connect(lambda v: self.image_opacity_value.setText(str(v)))
        
    def save_settings(self):
        """保存设置"""
        # 常规设置
        self.settings['auto_start'] = self.auto_start_cb.isChecked()
        self.settings['pick_mode'] = 'default' if self.mode_default_rb.isChecked() else 'random'
        
        # 动画效果
        anim_map = {0: 'ripple', 1: 'spark', 2: 'circle', 3: 'none'}
        self.settings['animation_style'] = anim_map.get(self.animation_combo.currentIndex(), 'ripple')
        
        # 按钮设置
        self.settings['button_text'] = self.button_text_edit.text() or 'Call'
        self.settings['button_opacity'] = self.opacity_slider.value()
        
        # 按钮样式
        style_map = {'纯色': 'solid', '渐变色': 'gradient', '金属质感': 'metal'}
        self.settings['button_style'] = style_map.get(self.button_style_combo.currentText(), 'gradient')
        
        # 弹窗设置
        self.settings['popup_font_family'] = self.font_combo.currentFont().family()
        self.settings['popup_text_opacity'] = self.text_opacity_slider.value()
        self.settings['popup_bg_opacity'] = self.bg_opacity_slider.value()
        
        # 弹窗背景样式
        self.settings['popup_bg_style'] = style_map.get(self.popup_style_combo.currentText(), 'gradient')
        
        self.settings['popup_use_bg_image'] = self.use_bg_image_cb.isChecked()
        self.settings['popup_bg_image_opacity'] = self.image_opacity_slider.value()
        
        # 更新父窗口的设置
        self.parent.settings.update(self.settings)
        self.parent.save_settings()
        self.parent.apply_settings()
        
        self.accept()

class AboutDialog(QDialog):
    """关于对话框"""
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("关于")
        self.setFixedSize(450, 200)
        
        # 设置窗口标志，移除问号按钮
        self.setWindowFlags(self.windowFlags() & ~Qt.WindowContextHelpButtonHint)
        
        # 设置窗口图标
        self.setWindowIcon(parent.windowIcon() if parent else QIcon())
        
        layout = QVBoxLayout()
        layout.setSpacing(10)
        
        # 创建文本并左对齐
        text = QLabel(
            "ClassRollCall-班级点名\n"
            "版本: 1.3（formal）\n"
            "作者: enderLuoxh\n"
        )
        text.setAlignment(Qt.AlignLeft)
        layout.addWidget(text)
        
        # 添加GitHub链接
        github_layout = QHBoxLayout()
        github_label = QLabel("项目地址: ")
        github_label.setAlignment(Qt.AlignLeft)
        
        github_link = QLabel('<a href="https://github.com/enderLuoxh/ClassRollCall">https://github.com/enderLuoxh/ClassRollCall</a>')
        github_link.setOpenExternalLinks(True)
        github_link.setAlignment(Qt.AlignLeft)
        
        github_layout.addWidget(github_label)
        github_layout.addWidget(github_link)
        github_layout.addStretch()
        
        layout.addLayout(github_layout)
        
        layout.addStretch()
        
        # 确定按钮
        btn_layout = QHBoxLayout()
        btn = QPushButton("确定")
        btn.clicked.connect(self.accept)
        btn_layout.addStretch()
        btn_layout.addWidget(btn)
        
        layout.addLayout(btn_layout)
        
        self.setLayout(layout)

class DraggableButton(QWidget):
    """可拖动的抽取按钮窗口（半透明样式）"""
    def __init__(self):
        super().__init__()
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)
        self.setAttribute(Qt.WA_TranslucentBackground)
        
        # 获取程序所在目录（解决打包后路径问题）
        if getattr(sys, 'frozen', False):
            # 如果是打包后的 exe 文件
            self.app_dir = os.path.dirname(sys.executable)
            self.app_path = sys.executable
        else:
            # 如果是脚本文件
            self.app_dir = os.path.dirname(os.path.abspath(__file__))
            self.app_path = os.path.abspath(__file__)
        
        # 获取用户应用数据目录（Local文件夹）
        self.app_data_dir = os.path.join(os.environ.get('LOCALAPPDATA', os.path.expanduser('~')), 'ClassRollCall')
        if not os.path.exists(self.app_data_dir):
            os.makedirs(self.app_data_dir)
            print(f"创建应用数据目录: {self.app_data_dir}")
        
        # 配置文件路径（在Local文件夹）
        self.position_file = os.path.join(self.app_data_dir, "position.json")
        self.settings_file = os.path.join(self.app_data_dir, "settings.json")
        
        # 名字文件路径（在程序目录）
        self.names_file = os.path.join(self.app_dir, "names.csv")
        
        # 默认设置
        self.settings = {
            'auto_start': False,
            'button_opacity': 180,
            'button_color1': [100, 150, 255],
            'button_color2': [200, 100, 255],
            'button_style': 'gradient',
            'button_text': 'Call',
            'pick_mode': 'default',
            'animation_style': 'ripple',
            'popup_font_family': 'Microsoft YaHei',
            'popup_text_color': [255, 255, 255],
            'popup_text_opacity': 255,
            'popup_bg_color1': [100, 150, 255],
            'popup_bg_color2': [200, 100, 255],
            'popup_bg_style': 'gradient',
            'popup_bg_opacity': 220,
            'popup_use_bg_image': False,
            'popup_bg_image_path': '',
            'popup_bg_image_opacity': 180
        }
        
        # 获取屏幕DPI缩放因子
        screen = QApplication.primaryScreen()
        self.dpi = screen.logicalDotsPerInch()
        self.scale_factor = self.dpi / 96.0
        
        # 获取屏幕分辨率
        screen_geometry = screen.geometry()
        self.screen_width = screen_geometry.width()
        self.screen_height = screen_geometry.height()
        
        # 固定按钮大小（物理尺寸：120x60像素，在96 DPI下）
        # 根据DPI缩放确保在不同分辨率下物理大小一致
        self.button_width = int(120 * self.scale_factor)
        self.button_height = int(60 * self.scale_factor)
        
        # 拖动边框大小（增加一点以便于拖动）
        self.drag_border = int(8 * self.scale_factor)
        
        # 窗口大小 = 按钮大小 + 拖动边框
        window_width = self.button_width + 2 * self.drag_border
        window_height = self.button_height + 2 * self.drag_border
        
        self.setFixedSize(window_width, window_height)
        
        # 变量初始化
        self.drag_position = None
        self.button_x = self.drag_border
        self.button_y = self.drag_border
        self.is_dragging = False
        self.drag_start_pos = None
        self.drag_threshold = int(5 * self.scale_factor)
        
        # 加载配置
        self.load_settings()
        self.load_position()
        self.load_names()
        
        # 初始化轮次
        self.reset_round()
        
        # 初始化动画效果
        self.ripple_effect = RippleEffect()
        
        # 设置UI
        self.setup_ui()
        
        # 创建系统托盘
        self.setup_tray()
        
        # 应用设置
        self.apply_settings()
        
    def get_app_path_for_registry(self):
        """获取用于注册表的应用程序路径"""
        if getattr(sys, 'frozen', False):
            # 如果是打包后的 exe 文件
            return f'"{sys.executable}"'
        else:
            # 如果是脚本文件
            return f'"{sys.executable}" "{os.path.abspath(__file__)}"'
        
    def handle_auto_start(self):
        """处理开机自启动（使用注册表方式，避免依赖win32com）"""
        try:
            key_path = r"Software\Microsoft\Windows\CurrentVersion\Run"
            
            if self.settings['auto_start']:
                # 添加开机自启动（使用注册表）
                key = reg.OpenKey(reg.HKEY_CURRENT_USER, key_path, 0, reg.KEY_SET_VALUE)
                reg.SetValueEx(key, "点名器", 0, reg.REG_SZ, self.get_app_path_for_registry())
                reg.CloseKey(key)
                print("开机自启动已启用")
            else:
                # 删除开机自启动
                try:
                    key = reg.OpenKey(reg.HKEY_CURRENT_USER, key_path, 0, reg.KEY_SET_VALUE)
                    reg.DeleteValue(key, "点名器")
                    reg.CloseKey(key)
                    print("开机自启动已禁用")
                except FileNotFoundError:
                    pass  # 如果不存在就忽略
        except Exception as e:
            print(f"处理开机自启动失败: {e}")
    
    def check_auto_start_status(self):
        """检查当前开机自启动状态"""
        try:
            key_path = r"Software\Microsoft\Windows\CurrentVersion\Run"
            key = reg.OpenKey(reg.HKEY_CURRENT_USER, key_path, 0, reg.KEY_READ)
            try:
                value, _ = reg.QueryValueEx(key, "点名器")
                reg.CloseKey(key)
                # 检查注册表中的值是否与当前程序路径匹配
                current_path = self.get_app_path_for_registry()
                return value == current_path
            except FileNotFoundError:
                reg.CloseKey(key)
                return False
        except Exception:
            return False
    
    def load_names(self):
        """加载名单 - 从自定义或默认CSV文件加载"""
        self.all_names = []
        
        # 检查是否有自定义名单文件
        custom_file = self.settings.get('custom_names_file', '')
        if custom_file and os.path.exists(custom_file):
            try:
                self.all_names = self.load_names_from_csv(custom_file)
                print(f"已从自定义文件加载 {len(self.all_names)} 个名字: {custom_file}")
            except Exception as e:
                print(f"加载自定义CSV文件失败: {e}")
                self.all_names = []
        
        # 如果没有自定义文件或加载失败，尝试加载默认文件
        if not self.all_names and os.path.exists(self.names_file):
            try:
                self.all_names = self.load_names_from_csv(self.names_file)
                print(f"已从默认文件加载 {len(self.all_names)} 个名字")
            except Exception as e:
                print(f"加载默认CSV文件失败: {e}")
                self.all_names = []
        elif not self.all_names:
            # 如果文件不存在或为空，创建空的CSV文件
            self.create_empty_csv(self.names_file)
            print(f"已创建空的CSV文件：{self.names_file}")
            
            # 弹出提示
            QTimer.singleShot(1000, self.show_empty_names_warning)
    
    def load_names_from_csv(self, filename):
        """从CSV文件加载名字"""
        names = []
        with open(filename, 'r', encoding='utf-8-sig') as f:
            reader = csv.reader(f)
            for row in reader:
                if row:  # 确保行不为空
                    # 取每行的第一个非空单元格作为名字
                    for cell in row:
                        cell = cell.strip()
                        if cell and not cell.startswith('#'):  # 跳过注释行
                            names.append(cell)
                            break  # 只取每行的第一个名字
        return names
    
    def save_names_to_csv(self, names, filename):
        """保存名字到CSV文件"""
        with open(filename, 'w', encoding='utf-8-sig', newline='') as f:
            writer = csv.writer(f)
            for name in names:
                writer.writerow([name])
    
    def create_empty_csv(self, filename):
        """创建空的CSV文件并添加说明"""
        with open(filename, 'w', encoding='utf-8-sig', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['# 请在下方添加名字，每行一个'])
            writer.writerow(['姓名1'])
            writer.writerow(['姓名2'])
            writer.writerow(['姓名3'])
    
    def show_empty_names_warning(self):
        """显示空名单警告"""
        msg = QMessageBox()
        msg.setIcon(QMessageBox.Warning)
        msg.setWindowTitle("提示")
        msg.setText("名单文件为空")
        msg.setInformativeText(f"请在以下文件中添加名字：\n\n{self.names_file}\n\n格式：每行一个名字")
        msg.setStandardButtons(QMessageBox.Ok)
        msg.exec_()
            
    def reset_round(self):
        """重置轮次"""
        if self.all_names:
            self.current_round = self.all_names.copy()
            random.shuffle(self.current_round)
            self.current_index = 0
            print("已重置轮次，重新洗牌")
        else:
            self.current_round = []
            self.current_index = 0
            
    def setup_ui(self):
        # 主布局
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        
        # 创建一个容器用于放置按钮
        container = QWidget()
        container.setAttribute(Qt.WA_TranslucentBackground)
        container_layout = QVBoxLayout()
        container_layout.setContentsMargins(
            self.button_x,
            self.button_y,
            self.button_x,
            self.button_y
        )
        
        # 抽取按钮
        self.button = QPushButton(self.settings.get('button_text', 'Call'))
        self.button.setFixedSize(self.button_width, self.button_height)
        
        # 字体大小：按钮高度的50%
        font_size = int(self.button_height * 0.5)
        
        # 设置按钮样式 - 基础样式
        self.button.setStyleSheet(f"""
            QPushButton {{
                font-size: {font_size}px;
                font-weight: bold;
                font-family: Microsoft YaHei;
                color: #FFFFFF;
                border: none;
                border-radius: {int(self.button_height * 0.15)}px;
                padding: 0px;
                margin: 0px;
                qproperty-alignment: AlignCenter;
            }}
        """)
        
        self.button.clicked.connect(self.pick_name)
        self.button.installEventFilter(self)
        
        container_layout.addWidget(self.button, 0, Qt.AlignCenter)
        container.setLayout(container_layout)
        
        layout.addWidget(container)
        self.setLayout(layout)
        
    def setup_tray(self):
        """设置系统托盘"""
        # 创建托盘图标
        self.tray_icon = QSystemTrayIcon(self)
        self.tray_icon.setToolTip("点名器")
        
        # 创建图标
        icon = QIcon()
        pixmap = QPixmap(64, 64)
        pixmap.fill(Qt.transparent)
        painter = QPainter(pixmap)
        painter.setBrush(QColor(100, 150, 255, 200))
        painter.setPen(Qt.NoPen)
        painter.drawRoundedRect(0, 0, 64, 64, 10, 10)
        painter.setPen(QColor(255, 255, 255))
        painter.setFont(QFont("Microsoft YaHei", 24, QFont.Bold))
        painter.drawText(pixmap.rect(), Qt.AlignCenter, "抽")
        painter.end()
        icon.addPixmap(pixmap)
        
        self.tray_icon.setIcon(icon)
        self.setWindowIcon(icon)  # 设置窗口图标
        
        # 创建托盘菜单
        tray_menu = QMenu()
        
        # 合并显示/隐藏为一个切换按钮
        self.toggle_action = tray_menu.addAction("隐藏窗口")
        self.toggle_action.triggered.connect(self.toggle_window)
        
        # 添加设置按钮
        settings_action = tray_menu.addAction("设置")
        settings_action.triggered.connect(self.show_settings)
        
        # 添加关于按钮
        about_action = tray_menu.addAction("关于")
        about_action.triggered.connect(self.show_about)
        
        tray_menu.addSeparator()
        
        quit_action = tray_menu.addAction("退出程序")
        quit_action.triggered.connect(self.quit_app)
        
        self.tray_icon.setContextMenu(tray_menu)
        
        # 托盘图标点击事件
        self.tray_icon.activated.connect(self.tray_icon_activated)
        
        # 显示托盘图标
        self.tray_icon.show()
        
    def tray_icon_activated(self, reason):
        """托盘图标被激活"""
        if reason == QSystemTrayIcon.DoubleClick:
            self.toggle_window()
    
    def toggle_window(self):
        """切换窗口显示/隐藏"""
        if self.isVisible():
            self.hide()
            self.toggle_action.setText("显示窗口")
        else:
            self.show()
            self.raise_()
            self.activateWindow()
            self.toggle_action.setText("隐藏窗口")
    
    def show_settings(self):
        """显示设置对话框"""
        dialog = SettingsDialog(self)
        dialog.exec_()
        # 重新加载名单（可能更改了文件）
        self.load_names()
        self.reset_round()
    
    def show_about(self):
        """显示关于对话框"""
        dialog = AboutDialog(self)
        dialog.exec_()
    
    def hide_window(self):
        """隐藏窗口"""
        self.hide()
        self.toggle_action.setText("显示窗口")
        self.tray_icon.showMessage(
            "点名器",
            "程序已最小化到系统托盘",
            QSystemTrayIcon.Information,
            2000
        )
    
    def quit_app(self):
        """退出程序"""
        self.save_position()
        self.save_settings()
        self.tray_icon.hide()
        QApplication.quit()
        
    def load_settings(self):
        """加载设置"""
        try:
            if os.path.exists(self.settings_file):
                with open(self.settings_file, "r", encoding="utf-8") as f:
                    loaded_settings = json.load(f)
                    self.settings.update(loaded_settings)
                print(f"已从 {self.settings_file} 加载设置")
                
                # 验证并修复开机自启动状态
                if self.settings.get('auto_start', False):
                    if not self.check_auto_start_status():
                        print("检测到开机自启动状态不一致，正在修复...")
                        self.handle_auto_start()
        except Exception as e:
            print(f"加载设置失败: {e}")
    
    def save_settings(self):
        """保存设置"""
        try:
            with open(self.settings_file, "w", encoding="utf-8") as f:
                json.dump(self.settings, f, indent=2)
            print(f"已保存设置到 {self.settings_file}")
        except Exception as e:
            print(f"保存设置失败: {e}")
    
    def apply_settings(self):
        """应用设置到UI"""
        # 更新按钮文字
        self.button.setText(self.settings.get('button_text', 'Call'))
        
        # 更新按钮样式
        self.update_button_style()
        
        # 更新动画样式
        self.ripple_effect.set_animation_style(self.settings.get('animation_style', 'ripple'))
        
        # 处理开机自启动
        self.handle_auto_start()
        
    def update_button_style(self):
        """更新按钮样式（支持渐变色和金属质感）"""
        font_size = int(self.button_height * 0.5)
        button_style = self.settings.get('button_style', 'gradient')
        opacity = self.settings.get('button_opacity', 180)
        color1 = self.settings.get('button_color1', [100, 150, 255])
        color2 = self.settings.get('button_color2', [200, 100, 255])
        
        if button_style == 'gradient':
            # 渐变色 - 修复渐变问题，确保两个颜色都正确显示
            style = f"""
                QPushButton {{
                    font-size: {font_size}px;
                    font-weight: bold;
                    font-family: Microsoft YaHei;
                    color: #FFFFFF;
                    border: none;
                    border-radius: {int(self.button_height * 0.15)}px;
                    padding: 0px;
                    margin: 0px;
                    qproperty-alignment: AlignCenter;
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                        stop:0 rgba({color1[0]}, {color1[1]}, {color1[2]}, {opacity}),
                        stop:1 rgba({color2[0]}, {color2[1]}, {color2[2]}, {opacity}));
                }}
                QPushButton:hover {{
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                        stop:0 rgba({min(color1[0] + 20, 255)}, {min(color1[1] + 20, 255)}, {min(color1[2] + 20, 255)}, {min(opacity + 40, 255)}),
                        stop:1 rgba({min(color2[0] + 20, 255)}, {min(color2[1] + 20, 255)}, {min(color2[2] + 20, 255)}, {min(opacity + 40, 255)}));
                }}
            """
        elif button_style == 'metal':
            # 金属质感
            style = f"""
                QPushButton {{
                    font-size: {font_size}px;
                    font-weight: bold;
                    font-family: Microsoft YaHei;
                    color: #333333;
                    border: 2px solid #AAAAAA;
                    border-radius: {int(self.button_height * 0.15)}px;
                    padding: 0px;
                    margin: 0px;
                    qproperty-alignment: AlignCenter;
                    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                        stop:0 rgba(240, 240, 240, {opacity}),
                        stop:0.5 rgba(200, 200, 200, {opacity}),
                        stop:1 rgba(160, 160, 160, {opacity}));
                }}
                QPushButton:hover {{
                    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                        stop:0 rgba(255, 255, 255, {min(opacity + 40, 255)}),
                        stop:0.5 rgba(220, 220, 220, {min(opacity + 40, 255)}),
                        stop:1 rgba(180, 180, 180, {min(opacity + 40, 255)}));
                }}
            """
        else:
            # 纯色
            style = f"""
                QPushButton {{
                    font-size: {font_size}px;
                    font-weight: bold;
                    font-family: Microsoft YaHei;
                    color: #FFFFFF;
                    border: none;
                    border-radius: {int(self.button_height * 0.15)}px;
                    padding: 0px;
                    margin: 0px;
                    qproperty-alignment: AlignCenter;
                    background-color: rgba({color1[0]}, {color1[1]}, {color1[2]}, {opacity});
                }}
                QPushButton:hover {{
                    background-color: rgba({min(color1[0] + 20, 255)}, {min(color1[1] + 20, 255)}, {min(color1[2] + 20, 255)}, {min(opacity + 40, 255)});
                }}
            """
        
        self.button.setStyleSheet(style)
        
    def load_position(self):
        """加载上次保存的位置"""
        try:
            if os.path.exists(self.position_file):
                with open(self.position_file, "r", encoding="utf-8") as f:
                    pos = json.load(f)
                    # 确保位置在屏幕内
                    x = min(max(pos["x"], 0), self.screen_width - self.width())
                    y = min(max(pos["y"], 0), self.screen_height - self.height())
                    self.move(x, y)
                print(f"已从 {self.position_file} 加载位置: ({x}, {y})")
                return
        except Exception as e:
            print(f"加载位置失败: {e}")
            pass
        
        # 默认位置：屏幕右下角
        self.move_to_default_position()
            
    def move_to_default_position(self):
        """移动到默认位置（屏幕右下角）"""
        default_x = self.screen_width - self.width() - int(20 * self.scale_factor)
        default_y = self.screen_height - self.height() - int(20 * self.scale_factor)
        self.move(default_x, default_y)
        print(f"使用默认位置: ({default_x}, {default_y})")
        self.save_position()
            
    def save_position(self):
        """保存当前位置"""
        try:
            pos = {
                "x": self.x(),
                "y": self.y()
            }
            with open(self.position_file, "w", encoding="utf-8") as f:
                json.dump(pos, f, indent=2)
            print(f"已保存位置到 {self.position_file}: ({pos['x']}, {pos['y']})")
        except Exception as e:
            print(f"保存位置失败: {e}")
            pass
        
    def eventFilter(self, obj, event):
        """事件过滤器，用于处理按钮上的鼠标事件"""
        if obj == self.button:
            if event.type() == QEvent.MouseButtonPress:
                self.drag_start_pos = event.pos()
                self.is_dragging = False
                return False
                
            elif event.type() == QEvent.MouseMove:
                if event.buttons() & Qt.LeftButton and self.drag_start_pos:
                    move_distance = (event.pos() - self.drag_start_pos).manhattanLength()
                    
                    if move_distance > self.drag_threshold:
                        self.is_dragging = True
                        button_global_pos = self.button.mapToGlobal(self.drag_start_pos)
                        self.drag_position = button_global_pos - self.pos()
                        self.mouseMoveEvent(event)
                        return True
                        
                return False
                
            elif event.type() == QEvent.MouseButtonRelease:
                if self.is_dragging:
                    self.is_dragging = False
                    self.drag_start_pos = None
                    self.drag_position = None
                    self.save_position()
                    return True
                    
                self.drag_start_pos = None
                return False
                
        return super().eventFilter(obj, event)
        
    def pick_name(self):
        """抽取名字"""
        if not self.all_names:
            print("名单为空，无法抽取")
            self.show_empty_names_warning()
            return
        
        # 根据模式选择名字
        if self.settings.get('pick_mode', 'default') == 'random':
            # 真随机模式：每次随机选择一个
            name = random.choice(self.all_names)
            print(f"真随机抽取: {name}")
        else:
            # 默认模式：轮次抽取
            if self.current_index >= len(self.current_round):
                self.reset_round()
            
            name = self.current_round[self.current_index]
            self.current_index += 1
            print(f"顺序抽取: {name} (第 {self.current_index}/{len(self.current_round)})")
        
        # 显示毛玻璃弹窗
        self.popup = GlassPopup(name, self.settings)
        self.popup.show()
        
        # 如果启用了动画，显示动画效果
        animation_style = self.settings.get('animation_style', 'ripple')
        if animation_style != 'none':
            # 在屏幕中央添加动画
            center = QPoint(self.screen_width // 2, self.screen_height // 2)
            self.ripple_effect.add_animation(center)
            self.ripple_effect.show()
            self.ripple_effect.raise_()
        
    def mousePressEvent(self, event):
        """处理窗口本身的拖动开始"""
        click_x = event.pos().x()
        click_y = event.pos().y()
        
        in_button_area = (
            click_x >= self.button_x and 
            click_x <= self.button_x + self.button_width and
            click_y >= self.button_y and 
            click_y <= self.button_y + self.button_height
        )
        
        if not in_button_area:
            if event.button() == Qt.LeftButton:
                self.drag_position = event.globalPos() - self.pos()
                return
        
        self.drag_start_pos = event.pos()
        self.is_dragging = False
            
    def mouseMoveEvent(self, event):
        """处理拖动"""
        if self.drag_position is not None:
            new_pos = event.globalPos() - self.drag_position
            
            max_x = self.screen_width - self.width()
            max_y = self.screen_height - self.height()
            
            new_x = max(0, min(new_pos.x(), max_x))
            new_y = max(0, min(new_pos.y(), max_y))
            
            self.move(new_x, new_y)
            
    def mouseReleaseEvent(self, event):
        """处理拖动结束"""
        if event.button() == Qt.LeftButton:
            self.drag_position = None
            self.save_position()
            
    def paintEvent(self, event):
        """完全透明，不绘制任何内容"""
        pass
            
    def closeEvent(self, event):
        """重写关闭事件，改为隐藏到系统托盘"""
        event.ignore()
        self.hide()
        self.toggle_action.setText("显示窗口")
        self.tray_icon.showMessage(
            "点名器",
            "程序已最小化到系统托盘",
            QSystemTrayIcon.Information,
            2000
        )

def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    
    # 设置应用程序名称（用于托盘图标）
    app.setApplicationName("点名")
    
    # 设置字体
    font = QFont("Microsoft YaHei", 10)
    app.setFont(font)
    
    # 检查系统托盘是否支持
    if not QSystemTrayIcon.isSystemTrayAvailable():
        QMessageBox.critical(None, "错误", "系统托盘不可用！")
        sys.exit(1)
    
    window = DraggableButton()
    window.setWindowTitle("点名")
    window.show()
    
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()
