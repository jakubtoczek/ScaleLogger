DARK_STYLESHEET = """
QWidget {
    background-color: #11161d;
    color: #e2e8f0;
    font-family: 'Segoe UI';
    font-size: 10.5pt;
}
QMainWindow {
    background-color: #11161d;
}
QLabel#sectionLabel {
    color: #cbd5e1;
    font-weight: 600;
}
QPushButton, QToolButton {
    background-color: #1f2937;
    border: 1px solid #334155;
    border-radius: 10px;
    padding: 6px 14px;
}
QPushButton:hover, QToolButton:hover {
    background-color: #293548;
}
QPushButton:checked {
    background-color: #2563eb;
    border-color: #2563eb;
}
QToolButton#toolbarIconButton {
    min-width: 34px;
    min-height: 34px;
    padding: 0px;
    font-size: 13pt;
}
QTextEdit#logView, QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QTabWidget::pane {
    background-color: #0f172a;
    border: 1px solid #334155;
    border-radius: 10px;
}
QTextEdit#logView {
    padding: 6px;
}
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    padding: 4px 8px;
}
QGroupBox {
    border: 1px solid #334155;
    border-radius: 10px;
    margin-top: 10px;
    padding-top: 12px;
}
QGroupBox::title {
    left: 10px;
    padding: 0 4px;
}
QTabBar::tab {
    background-color: #162131;
    border: 1px solid #334155;
    border-bottom: none;
    padding: 8px 12px;
    border-top-left-radius: 8px;
    border-top-right-radius: 8px;
}
QTabBar::tab:selected {
    background-color: #1f2937;
}
"""
