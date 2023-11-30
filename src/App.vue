<script setup lang="ts">
import { reactive, ref } from 'vue'
import {ElLoading, ElMessage} from "element-plus";
import Loading from "./components/Loading.vue";
import Form from "./components/Form.vue";

const showLoading = ref(false)
const dialogFormVisible = ref(true)

const password = ref('')



const fun = ()=>{
  const ret1 = window.ipcRenderer.sendSync('cli','cat /proc/devices|grep char_dev')
  console.log(ret1)
  if(!ret1.stdout){
    console.log('加载模块')
    window.ipcRenderer.sendSync('cli','echo '+password.value+'|sudo -S insmod mymodule.ko')
  }
  window.ipcRenderer.sendSync('cli','echo '+password.value+'|sudo -S dmesg --clear')
  const ret2 = window.ipcRenderer.sendSync('cli','echo '+password.value+'|sudo -S dmesg|grep mymodule')
  console.log(ret2)
  const ret3 = window.ipcRenderer.sendSync('cli','echo '+password.value+'|sudo -S ./test_module 测试')
  console.log(ret3)
  const ret4 = window.ipcRenderer.sendSync('cli','echo '+password.value+'|sudo -S dmesg|grep mymodule')
  console.log(ret4)
}

const dialogConfirmHandler = () =>{
  showLoading.value = true
  setTimeout(()=>{
    if(!window.ipcRenderer.sendSync('cli', 'echo '+password.value+'|sudo -S ls').stdout){
      showLoading.value = false
      ElMessage.error("密码错误，请重新输入！")
      return
    }
    showLoading.value = false
    dialogFormVisible.value = false
    fun()
  },50)

}
</script>

<template>
  <Form/>
<!--  <el-dialog v-model="dialogFormVisible" :show-close="false">-->
<!--    <el-text class="mx-1" type="primary">Linux加载内核模块需要sudo权限</el-text>-->
<!--    <el-input-->
<!--        v-model="password"-->
<!--        type="password"-->
<!--        :placeholder="'请输入当前用户'+currentUserName+'的登录密码'"-->
<!--        show-password-->
<!--        autocomplete="off"-->
<!--        modal-->
<!--        @keyup.enter="dialogConfirmHandler"-->
<!--    />-->
<!--    <template #footer>-->
<!--      <span class="dialog-footer">-->
<!--        <el-button type="primary" @click="dialogConfirmHandler">-->
<!--          确认-->
<!--        </el-button>-->
<!--      </span>-->
<!--    </template>-->
<!--  </el-dialog>-->
  <Loading v-show="showLoading"/>

</template>

<style scoped>
.logo {
  height: 6em;
  padding: 1.5em;
  will-change: filter;
  transition: filter 300ms;
}
.logo:hover {
  filter: drop-shadow(0 0 2em #646cffaa);
}
.logo.vue:hover {
  filter: drop-shadow(0 0 2em #42b883aa);
}
.el-button--text {
  margin-right: 15px;
}
.el-select {
  width: 300px;
}
.el-input {
  width: 300px;
}
.dialog-footer button:first-child {
  margin-right: 10px;
}
</style>