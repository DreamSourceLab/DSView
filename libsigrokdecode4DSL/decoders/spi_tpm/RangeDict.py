# Source: https://raw.githubusercontent.com/WKPlus/rangedict/master/rangedict.py
__all__ = ['RangeDict']


class Color(object):
    BLACK = 0
    RED = 1


class Node(object):
    __slots__ = ('r', 'left', 'right', 'value', 'color', 'parent')

    def __init__(self, r, value, parent=None, color=Color.RED):
        self.r = r
        self.value = value
        self.parent = parent
        self.color = color
        self.left = None
        self.right = None

    def value_copy(self, other):
        self.r = other.r
        self.value = other.value


class RangeDict(dict):

    def __init__(self):
        self._root = None

    def __setitem__(self, r, v):
        if r[1] < r[0]:
            raise KeyError
        node = self._insert(r, v)
        self._insert_adjust(node)

    def _insert(self, r, v):
        if not self._root:
            self._root = Node(r, v)
            return self._root
        cur = self._root
        while True:
            if r[1] < cur.r[0]:
                if not cur.left:
                    cur.left = Node(r, v, cur)
                    return cur.left
                cur = cur.left
            elif r[0] > cur.r[1]:
                if not cur.right:
                    cur.right = Node(r, v, cur)
                    return cur.right
                cur = cur.right
            else:
                raise KeyError  # overlap not supported

    def _insert_adjust(self, node):
        ''' adjust to make the tree still a red black tree '''
        if not node.parent:
            node.color = Color.BLACK
            return
        if node.parent.color == Color.BLACK:
            return
        uncle = self.sibling(node.parent)
        if node_color(uncle) == Color.RED:
            node.parent.color = Color.BLACK
            uncle.color = Color.BLACK
            node.parent.parent.color = Color.RED
            return self._insert_adjust(node.parent.parent)

        #parent is red and uncle is black
        # since parent is red, grandparent must exists and be black
        parent = node.parent
        grandparent = parent.parent
        if self.is_left_son(parent, grandparent):
            if self.is_left_son(node, parent):
                self.right_rotate(grandparent)
                grandparent.color = Color.RED
                parent.color = Color.BLACK
            else:
                self.left_rotate(parent)
                self.right_rotate(grandparent)
                grandparent.color = Color.RED
                node.color = Color.BLACK
        else:
            if self.is_left_son(node, parent):
                self.right_rotate(parent)
                self.left_rotate(grandparent)
                grandparent.color = Color.RED
                node.color = Color.BLACK
            else:
                self.left_rotate(grandparent)
                grandparent.color = Color.RED
                parent.color = Color.BLACK

    def _find_key(self, key):
        cur = self._root
        while cur:
            if key > cur.r[1]:
                cur = cur.right
            elif key < cur.r[0]:
                cur = cur.left
            else:
                break
        return cur

    def _find_range(self, r):
        cur = self._root
        while cur:
            if r[1] < cur.r[0]:
                cur = cur.left
            elif r[0] > cur.r[1]:
                cur = cur.right
            elif r[0] == cur.r[0] and r[1] == cur.r[1]:
                return cur
            else:
                raise KeyError
        raise KeyError

    def __getitem__(self, key):
        tar = self._find_key(key)
        if tar:
            return tar.value
        raise KeyError

    def __contains__(self, key):
        return bool(self._find_key(key))

    def __delitem__(self, r):
        node = self._find_range(r)
        if node.left and node.right:
            left_rightest_child = self.find_rightest(node.left)
            node.value_copy(left_rightest_child)
            node = left_rightest_child
        self._delete(node)

    def _delete(self, node):
        # node has at most one child
        child = node.left if node.left else node.right
        if not node.parent:  # node is root
            self._root = child
            if self._root:
                self._root.parent = None
                self._root.color = Color.BLACK
            return

        parent = node.parent
        if not child:
            child = Node(None, None, parent, Color.BLACK)
        if self.is_left_son(node, parent):
            parent.left = child
        else:
            parent.right = child
        child.parent = parent

        if node.color == Color.RED:
            # no need to adjust when deleting a red node
            return
        if node_color(child) == Color.RED:
            child.color = Color.BLACK
            return
        self._delete_adjust(child)
        if not child.r:
            # mock a None node for adjust, need to delete it after that
            parent = child.parent
            if self.is_left_son(child, parent):
                parent.left = None
            else:
                parent.right = None

    def _delete_adjust(self, node):
        if not node.parent:
            node.color = Color.BLACK
            return

        parent = node.parent
        sibling = self.sibling(node)
        if node_color(sibling) == Color.RED:
            if self.is_left_son(node, parent):
                self.left_rotate(parent)
            else:
                self.right_rotate(parent)
            parent.color = Color.RED
            sibling.color = Color.BLACK
            sibling = self.sibling(node)  # must be black

        # sibling must be black now
        if not self.is_black(parent) and self.is_black(sibling.left) and \
           self.is_black(sibling.right):
            parent.color = Color.BLACK
            sibling.color = Color.RED
            return

        if self.is_black(parent) and self.is_black(sibling.left) and \
           self.is_black(sibling.right):
            sibling.color = Color.RED
            return self._delete_adjust(parent)

        if self.is_left_son(node, parent):
            if not self.is_black(sibling.left) and \
               self.is_black(sibling.right):
                sibling.left.color = Color.BLACK
                sibling.color = Color.RED
                self.right_rotate(sibling)
                sibling = sibling.parent

            # sibling.right must be red
            sibling.color = parent.color
            parent.color = Color.BLACK
            sibling.right.color = Color.BLACK
            self.left_rotate(parent)
        else:
            if not self.is_black(sibling.right) and \
               self.is_black(sibling.left):
                sibling.right.color = Color.BLACK
                sibling.color = Color.RED
                self.left_rotate(parent)
                sibling = sibling.parent

            # sibling.left must be red
            sibling.color = parent.color
            parent.color = Color.BLACK
            sibling.left.color = Color.RED
            self.right_rotate(parent)

    def left_rotate(self, node):
        right_son = node.right

        if not node.parent:
            self._root = right_son
        elif self.is_left_son(node, node.parent):
            node.parent.left = right_son
        else:
            node.parent.right = right_son
        right_son.parent = node.parent

        node.parent = right_son
        node.right = right_son.left
        right_son.left = node

    def right_rotate(self, node):
        left_son = node.left
        if not node.parent:
            self._root = left_son
        elif self.is_left_son(node, node.parent):
            node.parent.left = left_son
        else:
            node.parent.right = left_son
        left_son.parent = node.parent

        node.parent = left_son
        node.left = left_son.right
        left_son.right = node

    @staticmethod
    def sibling(node):
        if node.parent.left == node:
            return node.parent.right
        else:
            return node.parent.left

    @staticmethod
    def is_left_son(child, parent):
        if parent.left == child:
            return True
        else:
            return False

    @staticmethod
    def find_rightest(node):
        while node.right:
            node = node.right
        return node

    @staticmethod
    def is_black(node):
        return node_color(node) == Color.BLACK


def node_color(node):
    if not node:
        return Color.BLACK
    return node.color


def in_order(root):
    ret = []
    if not root:
        return []
    return in_order(root.left) + [root.value] + in_order(root.right)


def height(root):
    if not root:
        return 0
    return 1 + max(height(root.left), height(root.right))


if __name__ == '__main__':
    pass
