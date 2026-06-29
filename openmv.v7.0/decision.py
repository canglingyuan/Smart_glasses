"""
智能助盲眼镜 v7.0 · 综合决策引擎
=================================
综合状态机与优先级仲裁 + 软投票紧急制动。

优先级:
  4 - 红灯 / 头顶障碍物
  3 - 坑洞/凸起 / 大型障碍物 / 横向拦截物
  2 - 盲道偏离 (视觉追踪)
  1 - 斑马线 / 楼梯
  0 - 绿灯 / 转弯建议

注: 颜色传感器已移除，机动车道检测 (原P5) 和颜色传感器盲道检测 (原P2) 已删除。
    盲道检测改用摄像头视觉追踪 (vision.py TactileTracker)。
"""


class DecisionEngine:
    """多事件优先级仲裁 + 软投票紧急制动"""

    def __init__(self, cfg):
        self.cfg = cfg

    def evaluate(self, avg_dist, light, crosswalk, cross_offset,
                 obstacle_blob, obstacle_area, lateral_line,
                 pothole, stairs, overhead_danger, turn_advice,
                 tactile_direction='none'):
        """
        返回: (event_type, description, priority, turn_advice)

        v7.0: 移除斑马线末端/接近/偏离, 只保留基本斑马线。
            tactical_direction 替代原 TACTILE 检测 (来自 vision.py)。
        """
        cfg = self.cfg
        events = []  # (priority, event_type, desc)

        # 4 - 红灯
        if light == 'red':
            events.append((4, 'red', '红灯，请停下'))

        # 4 - 头顶障碍物
        if overhead_danger and avg_dist < cfg.OVERHEAD_DIST_THRESH:
            events.append((4, 'overhead', '头顶障碍物，请低头'))

        # 3 - 坑洞/凸起
        if pothole in ('pothole', 'bump'):
            desc = '危险！前方坑洞' if pothole == 'pothole' else '小心路面凸起'
            events.append((3, pothole, desc))

        # 3 - 大型障碍物
        if (obstacle_blob is not None
                and obstacle_area > cfg.AREA_BLOCK
                and avg_dist < cfg.DIST_BLOCK):
            events.append((3, 'obstacle', '大型障碍物阻挡'))
        elif (obstacle_blob is not None
                and obstacle_area > cfg.AREA_CAUTION
                and avg_dist < cfg.DIST_CAUTION):
            events.append((2, 'obstacle_near', '前方有障碍物%.0f厘米' % avg_dist))

        # 3 - 横向拦截物
        if lateral_line and avg_dist < 100:
            events.append((3, 'lateral', '横向拦截物，请绕行'))

        # 2 - 盲道偏离 (视觉追踪)
        if tactile_direction in ('left', 'right'):
            guidance = '偏左，请向右调整' if tactile_direction == 'left' else '偏右，请向左调整'
            events.append((2, 'tactile_warn', guidance))

        # 1 - 斑马线
        if crosswalk and light != 'red':
            events.append((1, 'crosswalk', '斑马线区域'))

        # 0 - 绿灯
        if light == 'green':
            events.append((0, 'green', '绿灯可通行'))

        # 1 - 楼梯
        if stairs == 'up':
            events.append((1, 'stairs_up', '前方上楼梯'))
        elif stairs == 'down':
            events.append((3, 'stairs_down', '危险！前方下楼梯'))  # 下楼是P3
        elif stairs == 'potential':
            events.append((1, 'stairs_up', '前方疑似楼梯'))

        # 0 - 转弯建议
        if turn_advice in ('left', 'right') and not events:
            direction_word = '左' if turn_advice == 'left' else '右'
            events.append((0, turn_advice, '请向%s绕行' % direction_word))

        if not events:
            return ('clean', '前方畅通', 0, 'none')

        # 排序: 优先级降序, 同优先级按预定顺序 (obstacle > pothole > crosswalk_end > lateral > tactile > crosswalk > stairs > green > turn)
        event_order = {'obstacle': 0, 'lateral': 1, 'pothole': 2, 'bump': 2,
                       'obstacle_near': 5, 'tactile_warn': 6,
                       'stairs_down': 10,
                       'crosswalk': 12, 'stairs_up': 13, 'green': 14, 'left': 15, 'right': 16}
        events.sort(key=lambda x: (-x[0], event_order.get(x[1], 99)))
        top = events[0]
        return (top[1], top[2], top[0], turn_advice)
