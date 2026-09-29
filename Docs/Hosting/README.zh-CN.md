<!-- Copyright (c) 2026 Nelaric -->

[English](README.md) | 简体中文

# 组织文档托管

组织文档统一使用 `https://docs.nelaric.com/<仓库名>/<栏目>/`。当前项目的 API 文档地址为 `https://docs.nelaric.com/nelaric-unreal-gameplay/API/`；`API` 区分大小写。生成的页面和资源一起放在该目录中。

## GitHub Pages 结构

组织站点仓库 `Nelaric/nelaric.github.io` 绑定自定义域名 `docs.nelaric.com`。项目仓库继承这个域名，并保留仓库名路径。`Nelaric/nelaric-unreal-gameplay` 的自定义域名应留空。

项目的 API Pages 工作流生成 Doxygen HTML，将完整产物复制到 Pages 发布包的 `API/` 目录，并在项目根目录添加跳转到 `API/` 的入口。因此，迁移后原有的 GitHub Pages 项目根地址仍可作为入口。原来位于根目录的具体 HTML 页面深层链接不由这个入口重定向。

## 首次配置

1. 创建公开的组织站点仓库 `Nelaric/nelaric.github.io`，把 `Setup/OrganizationPages/` 中的内容复制到其根目录，包括 `.nojekyll`。这些文件是发布模板；`CNAME` 必须只包含域名，不能添加版权注释。
2. 在组织站点启用 GitHub Pages，发布源选择默认分支的根目录。在添加 DNS 记录前，将自定义域名设为 `docs.nelaric.com`。根页面打开当前项目的 API 文档；以后发布更多项目时，可以替换为组织文档索引。
3. 在 `nelaric.com` 的权威 DNS 服务商处添加下表中的记录。如果 DNS 托管在阿里云，使用云解析 DNS 的公网权威解析控制台。域名注册商与权威 DNS 服务商可能不同。
4. 从 `main` 发布本项目的 API Pages 工作流。项目的 Pages 发布源保持为 GitHub Actions，自定义域名留空。
5. DNS 校验和证书签发完成后，在组织站点启用 Enforce HTTPS。通过 HTTPS 检查组织根地址、项目根地址、API 首页、具体 API 页面，以及样式和脚本资源。

| DNS 字段 | 值 |
| --- | --- |
| 域名 | `nelaric.com` |
| 记录类型 | `CNAME` |
| 主机记录 | `docs` |
| 记录值 | `nelaric.github.io` |
| 解析请求来源 | 默认 |
| TTL | 600 秒，或服务商默认值 |

DNS 记录值仅填写主机名，仓库名和 API 路径由 GitHub Pages 提供。后续项目站点应发布各自的栏目目录，并继承组织域名。

## 参考资料

- [GitHub：跨仓库使用自定义域名](https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site/about-custom-domains-and-github-pages#using-a-custom-domain-across-multiple-repositories)
- [GitHub：管理自定义域名](https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site/managing-a-custom-domain-for-your-github-pages-site)
- [阿里云：添加公网 DNS 解析记录](https://help.aliyun.com/zh/dns/pubz-add-parsing-record)
