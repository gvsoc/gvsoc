import numpy as np
from time import sleep
from rich import print
from rich.style import Style
from rich.progress import Progress, BarColumn, TextColumn

def pad_right(strings):
    """Pad all strings in the list with spaces (right-aligned) to equal length."""
    if not strings:
        return []
    max_len = max(len(s) for s in strings)
    return [s.ljust(max_len) for s in strings]

# your gradient function
def rgb_gradient(percent: float) -> str:
    """Smooth red->yellow->green gradient across 0..100%."""
    p = max(0.0, min(100.0, float(percent)))
    if p < 50:
        # red (255,0,0) -> yellow (255,255,0)
        r, g, b = 255, int(255 * (p / 50)), 0
    else:
        # yellow (255,255,0) -> green (0,255,0)
        r = int(255 * (1 - (p - 50) / 50))
        g, b = 255, 0
    return f"rgb({r},{g},{b})"

def show_colorful_bar(bar_name, percent: float):
    bar_color = 'yellow'
    if "_TOTAL_" in bar_name:
        bar_color = 'cyan'
        pass
    with Progress(
        TextColumn("[bold " + bar_color + "]{task.description}"),
        BarColumn(bar_width=50),
        TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
        transient=False,  # keep the bar after completion
    ) as progress:
        task = progress.add_task(f"{bar_name}", total=100)
        color = rgb_gradient(percent)
        progress.columns[1].complete_style = Style(color=color)
        progress.update(task, completed=percent)
    pass

def show_breakdown(dict_dict, metric, unit = '', scale_div = 1):
    value_list = []
    key_list = []
    for k, v in dict_dict.items():
        key_list.append(k)
        value_list.append(v[metric])
        pass
    v_np = np.array(value_list)
    percent_list = (100 * v_np / v_np.sum()).tolist()
    value_string_list = [f": {(v / scale_div):.1f}" for v in value_list]

    #Add Summary
    value_string_list.append(f": {(v_np.sum() / scale_div):.1f}")
    key_list.append("_TOTAL_")
    percent_list.append(100)

    #Visualization
    value_string_list = pad_right(value_string_list)
    key_list = pad_right(key_list)
    for x in range(len(key_list)):
        show_colorful_bar(f"{key_list[x]} {value_string_list[x]} {unit}", percent_list[x])
        pass
    print("")
    pass

def show_key_flow(dict_in, color = 'green'):
    string = '[bold yellow]START[/bold yellow]'
    for key in dict_in:
        if "repeat" in dict_in[key]:
            repeat = dict_in[key]["repeat"]
            string += f"->[{color}]{key}(x{repeat})[/{color}]"
        else:
            string += f"->[{color}]{key}[/{color}]"
            pass
        pass
    string += '->[bold yellow]END[/bold yellow]'
    print(string)
    pass

if __name__ == '__main__':
    show_colorful_bar("aaa", percent=32.1)
    show_colorful_bar("asfass", percent=62.1)
