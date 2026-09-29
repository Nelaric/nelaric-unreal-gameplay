<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](README.zh-CN.md)

# Organization documentation hosting

Organization documentation uses `https://docs.nelaric.com/<repository>/<section>/`. The API site for this project uses `https://docs.nelaric.com/nelaric-unreal-gameplay/API/`; `API` is case-sensitive. Generated pages and assets remain together inside that directory.

## GitHub Pages topology

The organization site repository `Nelaric/nelaric.github.io` owns the custom domain `docs.nelaric.com`. Project repositories inherit that domain and retain their repository-name path. Leave the custom domain of `Nelaric/nelaric-unreal-gameplay` unset.

The project's API Pages workflow generates Doxygen HTML, copies the complete output into `API/` in the Pages artifact, and adds a project-root redirect to `API/`. The existing GitHub Pages project-root URL therefore remains an entry point after the migration. Existing links to individual HTML files at the former root are not redirected by this entry page.

## Initial setup

1. Create the public organization site repository `Nelaric/nelaric.github.io`. Copy the contents of `Setup/OrganizationPages/`, including `.nojekyll`, to its root. These files are a deployment template; the `CNAME` file cannot contain a copyright comment because GitHub requires a plain domain name.
2. Publish the organization site from its default branch, root directory, using GitHub Pages. Set its custom domain to `docs.nelaric.com` before adding the DNS record. The root page opens the current project's API site; replace it with an organization documentation index when more projects are published.
3. In the authoritative DNS provider for `nelaric.com`, add the record below. If Alibaba Cloud hosts the DNS zone, use its public DNS resolution console. The registration provider and authoritative DNS provider can differ.
4. Publish this project's API Pages workflow from `main`. Keep its Pages publishing source set to GitHub Actions and its custom domain unset.
5. After DNS validation and certificate issuance, enable Enforce HTTPS for the organization site. Verify the organization root, project root, API index, an individual API page, and its styles and scripts over HTTPS.

| DNS field | Value |
| --- | --- |
| Zone | `nelaric.com` |
| Type | `CNAME` |
| Host record | `docs` |
| Value | `nelaric.github.io` |
| Request source | Default |
| TTL | 600 seconds, or the provider's default |

The DNS value contains only the host name. Repository and API paths are supplied by GitHub Pages. Future project sites should publish their own section directories and inherit the organization's domain.

## References

- [GitHub: using a custom domain across multiple repositories](https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site/about-custom-domains-and-github-pages#using-a-custom-domain-across-multiple-repositories)
- [GitHub: managing a custom domain](https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site/managing-a-custom-domain-for-your-github-pages-site)
- [Alibaba Cloud: adding public DNS records](https://help.aliyun.com/zh/dns/pubz-add-parsing-record)
