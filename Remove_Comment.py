# coding=utf-8

import re


def Remove_Comment():
    infile = input('输入你要删除那个文件的注释：').encode('utf-8')
    outfile = input('删除注释后保存的文件名：').encode('utf-8')
    files = []

    # 删除空行以及单行注释
    with open(infile, 'rb') as f:
        # 读取文件
        while True:
            lines = f.readline().decode()
            # 如果内容没有字符串, 或单行的/* .. */注释， 则迭代出去(删除空行)
            if lines.isspace() or ('/*' in lines and '*/' in lines):
                continue
            # 判断是否读取到内容, 如果读取不到内容, 退出循环
            elif not lines:
                break
            try:
                # 清除多余的回车符
                lines = re.sub(r'(\r|\n)+', '\n', lines)
                # 清除注释, (// 类型的注释)
                lines = re.sub(r'(\/\/.*)$', '', lines)
                # 添加到列表
                if not lines.isspace():
                    files.append(lines)
            except UnicodeDecodeError as e:
                print('文件解码错误')
                return

    # 确保列表不为空才执行
    if len(files) != 0:
        # 清空文件
        with open(outfile, 'w') as f:
            f.write('')

        # 写入内容
        with open(outfile, 'a', encoding='utf-8') as f:
            for line in files:
                f.write(line)

    ### 清理多行的 /* ..... */ 注释
    # 读取文件
    with open(outfile, 'r', encoding='utf-8') as f:
        content = f.read()
    # 删除多行注释   正则表达式写错了 导致把多个注释块何为一个注释块
    content = re.sub(r'\/\*(\s|.)*?\*\/\n', '', content)

    # 写入文件
    with open(outfile, 'w', encoding='utf-8') as f:
        f.write(content)
    print('注释删除成功！！！')


if __name__ == '__main__':
    Remove_Comment()
